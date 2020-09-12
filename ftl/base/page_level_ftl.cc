// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 *         Junhyeok Jang <jhjang@camelab.org>
 */

#include "ftl/base/page_level_ftl.hh"

#include "ftl/allocator/abstract_allocator.hh"
#include "ftl/mapping/abstract_mapping.hh"

namespace SimpleSSD::FTL {

PageLevelFTL::PageLevelFTL(ObjectData &o, FTLObjectData &fo, FTL *p)
    : AbstractFTL(o, fo, p) {
  memset(&stat, 0, sizeof(stat));

  auto param = ftlobject.pMapping->getInfo();
  pageSize = param->pageSize;

  ftlobject.pMapping->getMappingSize(&minMappingSize);

  pendingList = std::vector<Request *>(minMappingSize, nullptr);

  pendingListBaseAddress = object.memory->allocate(
      minMappingSize * pageSize, Memory::MemoryType::DRAM,
      "FTL::PageLevelFTL::PendingRMWData");

  // Create events
  eventReadSubmit =
      createEvent([this](uint64_t, uint64_t d) { read_submit(d); },
                  "FTL::PageLevelFTL::eventReadSubmit");
  eventReadDone = createEvent([this](uint64_t, uint64_t d) { read_done(d); },
                              "FTL::PageLevelFTL::eventReadDone");

  eventWriteSubmit =
      createEvent([this](uint64_t, uint64_t d) { write_submit(d); },
                  "FTL::PageLevelFTL::eventWriteSubmit");
  eventWriteDone = createEvent([this](uint64_t, uint64_t d) { write_done(d); },
                               "FTL::PageLevelFTL::eventWriteDone");
  eventPartialReadSubmit =
      createEvent([this](uint64_t t, uint64_t d) { rmw_readSubmit(t, d); },
                  "FTL::PageLevelFTL::eventPartialReadSubmit");
  eventPartialReadDone =
      createEvent([this](uint64_t t, uint64_t d) { rmw_readDone(t, d); },
                  "FTL::PageLevelFTL::eventPartialReadDone");
  eventPartialWriteSubmit =
      createEvent([this](uint64_t t, uint64_t d) { rmw_writeSubmit(t, d); },
                  "FTL::PageLevelFTL::eventPartialWriteSubmit");
  eventPartialWriteDone =
      createEvent([this](uint64_t t, uint64_t d) { rmw_writeDone(t, d); },
                  "FTL::PageLevelFTL::eventPartialWriteDone");
  eventInvalidateSubmit =
      createEvent([this](uint64_t t, uint64_t d) { invalidate_submit(t, d); },
                  "FTL::PageLevelFTL::eventInvalidateSubmit");

  mergeReadModifyWrite = readConfigBoolean(Section::FlashTranslation,
                                           Config::Key::MergeReadModifyWrite);
}

PageLevelFTL::~PageLevelFTL() {}

std::list<SuperRequest>::iterator PageLevelFTL::getWriteContext(uint64_t tag) {
  auto iter = writeList.begin();

  for (; iter != writeList.end(); iter++) {
    if (iter->front()->getTag() == tag) {
      break;
    }
  }

  panic_if(iter == writeList.end(), "Unexpected write context.");

  return iter;
}

std::unordered_map<uint64_t, PageLevelFTL::ReadModifyWriteContext>::iterator
PageLevelFTL::getRMWContext(uint64_t tag) {
  auto iter = rmwList.find(tag);

  panic_if(iter == rmwList.end(), "Unexpected tag in read-modify-write.");

  return iter;
}

void PageLevelFTL::read(Request *cmd) {
  ftlobject.pMapping->readMapping(cmd, eventReadSubmit);
}

void PageLevelFTL::read_submit(uint64_t tag) {
  auto req = getRequest(tag);

  if (LIKELY(req->getResponse() == Response::Success)) {
    pFIL->read(FIL::Request(req, eventReadDone));
  }
  else {
    // Error while translation
    completeRequest(req);
  }
}

void PageLevelFTL::read_done(uint64_t tag) {
  auto req = getRequest(tag);

  completeRequest(req);
}

void PageLevelFTL::write(Request *cmd) {
  // if SSD is running out of free block, stall request.
  // Stalled requests will be continued after GC
  if (!ftlobject.pAllocator->checkFreeBlockExist()) {
    debugprint(Log::DebugID::FTL_PageLevel, "WRITE | STALL | TAG: %" PRIu64,
               cmd->getTag());
    stalledRequests.push_back(cmd);

    triggerGC();
    return;
  }

  CPU::Function fstat;
  CPU::markFunction(fstat);

  LPN lpn = cmd->getLPN();
  LPN slpn = cmd->getSLPN();
  uint32_t nlp = cmd->getNLP();

  LPN alignedBegin = getAlignedLPN(lpn);
  LPN alignedEnd = static_cast<LPN>(alignedBegin + minMappingSize);

  LPN chunkBegin = MAX(slpn, alignedBegin);
  LPN chunkEnd = MIN(static_cast<LPN>(slpn + nlp), alignedEnd);

  // Store to pending list
  pendingList.at(lpn - alignedBegin) = cmd;

  // Check cmd is final of current chunk
  if (lpn + 1 == chunkEnd) {
    // Not aligned to minMappingSize
    if (alignedBegin != chunkBegin || alignedEnd != chunkEnd) {
      debugprint(Log::DebugID::FTL_PageLevel,
                 "RMW | INSERT | REQUEST %" PRIu64 " - %" PRIu64
                 " | ALIGN %" PRIu64 " - %" PRIu64,
                 chunkBegin, chunkEnd, alignedBegin, alignedEnd);

      bool merged = false;

      if (mergeReadModifyWrite) {
        // merge request if there is other RMW request which access same PPN
        for (auto &iter : rmwList) {
          if (iter.second.alignedBegin == alignedBegin &&
              !iter.second.writePending) {
            auto pctx = new ReadModifyWriteContext();

            pctx->alignedBegin = alignedBegin;
            pctx->chunkBegin = chunkBegin;
            pctx->list = std::move(pendingList);
            iter.second.push_back(pctx);

            merged = true;
          }
        }
      }

      if (!merged) {
        auto firstreq = pendingList.at(chunkBegin - alignedBegin);
        uint64_t tag = firstreq->getTag();
        auto ret = rmwList.emplace(tag, ReadModifyWriteContext());

        panic_if(!ret.second, "Duplicated FTL write ID.");

        ret.first->second.list = std::move(pendingList);
        ret.first->second.alignedBegin = alignedBegin;
        ret.first->second.chunkBegin = chunkBegin;

        // Do read translation - no need for loop
        ftlobject.pMapping->readMapping(firstreq, eventPartialReadSubmit);

        stat.rmwCount++;
      }
      else {
        debugprint(Log::DebugID::FTL_PageLevel, "RMW | MERGED");

        stat.rmwMerged++;
      }
    }
    else {
      auto &ret = writeList.emplace_back(std::move(pendingList));

      // No need for loop
      ftlobject.pMapping->writeMapping(ret.front(), eventWriteSubmit);
    }

    pendingList = std::vector<Request *>(minMappingSize, nullptr);
  }

  scheduleFunction(CPU::CPUGroup::FlashTranslationLayer, InvalidEventID, fstat);
}

void PageLevelFTL::write_submit(uint64_t tag) {
  auto list = getWriteContext(tag);

  PPN ppn = list->front()->getPPN();
  uint32_t offset = 0;

  for (auto &req : *list) {
    if (LIKELY(req->getResponse() == Response::Success)) {
      pFIL->program(FIL::Request(static_cast<PPN>(ppn + offset),
                                 req->getDRAMAddress(), eventWriteDone,
                                 req->getTag()));
    }
    else {
      completeRequest(req);
    }

    offset++;
  }

  writeList.erase(list);

  triggerGC();
}

void PageLevelFTL::write_done(uint64_t tag) {
  auto req = getRequest(tag);

  completeRequest(req);
}

void PageLevelFTL::rmw_readSubmit(uint64_t now, uint64_t tag) {
  auto iter = getRMWContext(tag);
  auto &ctx = iter->second;

  ctx.beginAt = now;

  debugprint(Log::DebugID::FTL_PageLevel,
             "RMW | READ   | ALIGN %" PRIu64 " - %" PRIu64, ctx.alignedBegin,
             ctx.alignedBegin + minMappingSize);

  // Get first command
  uint64_t diff = ctx.chunkBegin - ctx.alignedBegin;
  auto cmd = ctx.list.at(diff);

  if (LIKELY(cmd->getResponse() == Response::Success)) {
    // Convert PPN to aligned
    PPN ppnBegin = static_cast<PPN>(cmd->getPPN() - diff);
    uint64_t offset = 0;

    for (auto &cmd : ctx.list) {
      if (!cmd) {
        pFIL->read(FIL::Request(ppnBegin,
                                pendingListBaseAddress + offset * pageSize,
                                eventPartialReadDone, tag));

        ctx.counter++;
      }

      ++ppnBegin;
      offset++;
    }

    stat.rmwReadPages += ctx.counter;
  }
  else {
    ctx.counter = 1;

    rmw_readDone(now, tag);
  }
}

void PageLevelFTL::rmw_readDone(uint64_t now, uint64_t tag) {
  auto iter = getRMWContext(tag);
  auto &ctx = iter->second;

  ctx.counter--;

  if (ctx.counter == 0) {
    debugprint(Log::DebugID::FTL_PageLevel,
               "RMW | READ   | ALIGN %" PRIu64 " - %" PRIu64 " | %" PRIu64
               " - %" PRIu64 " (%" PRIu64 ")",
               ctx.alignedBegin, ctx.alignedBegin + minMappingSize, ctx.beginAt,
               now, now - ctx.beginAt);

    // Get first command
    auto cmd = ctx.list.at(ctx.chunkBegin - ctx.alignedBegin);

    // Write translation
    ftlobject.pMapping->writeMapping(cmd, eventPartialWriteSubmit);
  }
}

void PageLevelFTL::rmw_writeSubmit(uint64_t now, uint64_t tag) {
  auto iter = getRMWContext(tag);
  auto &ctx = iter->second;

  ctx.beginAt = now;
  ctx.writePending = true;

  debugprint(Log::DebugID::FTL_PageLevel,
             "RMW | WRITE  | ALIGN %" PRIu64 " - %" PRIu64, ctx.alignedBegin,
             ctx.alignedBegin + minMappingSize);

  // Get first command
  uint64_t diff = ctx.chunkBegin - ctx.alignedBegin;
  auto cmd = ctx.list.at(diff);

  if (LIKELY(cmd->getResponse() == Response::Success)) {
    // Convert PPN to aligned
    PPN ppnBegin = static_cast<PPN>(cmd->getPPN() - diff);
    uint64_t offset = 0;

    for (auto &cmd : ctx.list) {
      if (cmd) {
        pFIL->program(FIL::Request(ppnBegin, cmd->getDRAMAddress(),
                                   eventPartialWriteDone, tag));
      }
      else {
        pFIL->program(FIL::Request(ppnBegin,
                                   pendingListBaseAddress + offset * pageSize,
                                   eventPartialWriteDone, tag));
      }

      ctx.counter++;

      ++ppnBegin;
      offset++;
    }

    stat.rmwWrittenPages += ctx.counter;
  }
  else {
    ctx.counter = 1;

    rmw_writeDone(now, tag);
  }

  triggerGC();
}

void PageLevelFTL::rmw_writeDone(uint64_t now, uint64_t tag) {
  auto iter = getRMWContext(tag);
  auto &ctx = iter->second;

  ctx.counter--;

  if (ctx.counter == 0) {
    auto next = ctx.next;

    debugprint(Log::DebugID::FTL_PageLevel,
               "RMW | WRITE  | ALIGN %" PRIu64 " - %" PRIu64 " | %" PRIu64
               " - %" PRIu64 " (%" PRIu64 ")",
               ctx.alignedBegin, ctx.alignedBegin + minMappingSize, ctx.beginAt,
               now, now - ctx.beginAt);

    for (auto cmd : ctx.list) {
      if (cmd) {
        scheduleAbs(cmd->getEvent(), cmd->getEventData(), now);
      }
    }

    while (next) {
      auto cur = next;

      // Completion of all requests
      for (auto cmd : cur->list) {
        if (cmd) {
          scheduleAbs(cmd->getEvent(), cmd->getEventData(), now);
        }
      }

      next = cur->next;

      delete cur;
    }

    rmwList.erase(iter);
  }
}

void PageLevelFTL::invalidate(Request *cmd) {
  ftlobject.pMapping->invalidateMapping(cmd, eventInvalidateSubmit);
}

void PageLevelFTL::invalidate_submit(uint64_t, uint64_t tag) {
  auto req = getRequest(tag);

  warn("Trim and Format not implemented.");

  completeRequest(req);
}

void PageLevelFTL::backup(std::ostream &out, const SuperRequest &list) const
    noexcept {
  bool exist;
  uint64_t size = list.size();
  BACKUP_SCALAR(out, size);

  for (auto &iter : list) {
    exist = iter != nullptr;

    BACKUP_SCALAR(out, exist);

    if (exist) {
      uint64_t tag = iter->getTag();

      BACKUP_SCALAR(out, tag);
    }
  }
}

void PageLevelFTL::restore(std::istream &in, SuperRequest &list) noexcept {
  bool exist;
  uint64_t size;

  RESTORE_SCALAR(in, size);

  uint64_t tag;

  for (uint64_t i = 0; i < size; i++) {
    RESTORE_SCALAR(in, exist);

    if (exist) {
      RESTORE_SCALAR(in, tag);

      list.emplace_back(getRequest(tag));
    }
  }
}

void PageLevelFTL::getStatList(std::vector<Stat> &list,
                               std::string prefix) noexcept {
  list.emplace_back(prefix + "rmw.count", "Total read-modify-write operations");
  list.emplace_back(prefix + "rmw.merge_count",
                    "Total merged read-modify-write operations");
  list.emplace_back(prefix + "rmw.read_pages",
                    "Total read pages in read-modify-write");
  list.emplace_back(prefix + "rmw.written_pages",
                    "Total written pages in read-modify-write");
}

void PageLevelFTL::getStatValues(std::vector<double> &values) noexcept {
  values.push_back((double)stat.rmwCount);
  values.push_back((double)stat.rmwMerged);
  values.push_back((double)stat.rmwReadPages);
  values.push_back((double)stat.rmwWrittenPages);
}

void PageLevelFTL::resetStatValues() noexcept {
  memset(&stat, 0, sizeof(stat));
}

void PageLevelFTL::createCheckpoint(std::ostream &out) const noexcept {
  bool exist;
  uint64_t size;

  backup(out, pendingList);

  size = writeList.size();
  BACKUP_SCALAR(out, size);

  for (auto &iter : writeList) {
    backup(out, iter);
  }

  size = rmwList.size();
  BACKUP_SCALAR(out, size);

  for (auto &iter : rmwList) {
    BACKUP_SCALAR(out, iter.first);

    BACKUP_SCALAR(out, iter.second.alignedBegin);
    BACKUP_SCALAR(out, iter.second.chunkBegin);

    backup(out, iter.second.list);

    BACKUP_SCALAR(out, iter.second.writePending);
    BACKUP_SCALAR(out, iter.second.counter);

    exist = iter.second.next != nullptr;
    auto next = iter.second.next;

    while (true) {
      BACKUP_SCALAR(out, exist);

      if (exist) {
        BACKUP_SCALAR(out, next->alignedBegin);
        BACKUP_SCALAR(out, next->chunkBegin);

        backup(out, next->list);

        BACKUP_SCALAR(out, next->writePending);
        BACKUP_SCALAR(out, next->counter);

        next = next->next;
        exist = next != nullptr;
      }
      else {
        break;
      }
    }
  }

  BACKUP_SCALAR(out, stat);
  BACKUP_EVENT(out, eventReadSubmit);
  BACKUP_EVENT(out, eventReadDone);
  BACKUP_EVENT(out, eventWriteSubmit);
  BACKUP_EVENT(out, eventWriteDone);
  BACKUP_EVENT(out, eventInvalidateSubmit);
}

void PageLevelFTL::restoreCheckpoint(std::istream &in) noexcept {
  bool exist;
  uint64_t size;

  restore(in, pendingList);

  RESTORE_SCALAR(in, size);

  for (uint64_t i = 0; i < size; i++) {
    SuperRequest list;

    restore(in, list);

    writeList.emplace_back(std::move(list));
  }

  RESTORE_SCALAR(in, size);

  ReadModifyWriteContext cur;

  for (uint64_t i = 0; i < size; i++) {
    uint64_t tag;

    RESTORE_SCALAR(in, tag);

    RESTORE_SCALAR(in, cur.alignedBegin);
    RESTORE_SCALAR(in, cur.chunkBegin);

    restore(in, cur.list);

    RESTORE_SCALAR(in, cur.writePending);
    RESTORE_SCALAR(in, cur.counter);

    while (true) {
      RESTORE_SCALAR(in, exist);

      if (exist) {
        ReadModifyWriteContext *next = new ReadModifyWriteContext();

        RESTORE_SCALAR(in, next->alignedBegin);
        RESTORE_SCALAR(in, next->chunkBegin);

        restore(in, next->list);

        RESTORE_SCALAR(in, next->writePending);
        RESTORE_SCALAR(in, next->counter);

        cur.push_back(next);
      }
      else {
        break;
      }
    }

    rmwList.emplace(tag, cur);
  }

  RESTORE_SCALAR(in, stat);
  RESTORE_EVENT(in, eventReadSubmit);
  RESTORE_EVENT(in, eventReadDone);
  RESTORE_EVENT(in, eventWriteSubmit);
  RESTORE_EVENT(in, eventWriteDone);
  RESTORE_EVENT(in, eventInvalidateSubmit);
}

}  // namespace SimpleSSD::FTL
