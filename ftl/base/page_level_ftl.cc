// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "ftl/base/page_level_ftl.hh"

#include "ftl/allocator/abstract_allocator.hh"
#include "ftl/gc/abstract_gc.hh"
#include "ftl/mapping/abstract_mapping.hh"

namespace SimpleSSD::FTL {

PageLevelFTL::PageLevelFTL(ObjectData &o, FTLObjectData &fo, FTL *p,
                           FIL::FIL *f)
    : AbstractFTL(o, fo, p, f) {
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

std::unordered_map<uint64_t, ReadModifyWriteContext>::iterator
PageLevelFTL::getRMWContext(uint64_t tag) {
  auto iter = rmwList.find(tag);

  panic_if(iter == rmwList.end(), "Unexpected tag in read-modify-write.");

  return iter;
}

void PageLevelFTL::read(Request *cmd) {
  ftlobject.jobManager.trigger_readMapping(cmd);
  ftlobject.pMapping->readMapping(cmd, eventReadSubmit);
}

void PageLevelFTL::read_submit(uint64_t tag) {
  auto req = getRequest(tag);

  if (LIKELY(req->getResponse() == Response::Success)) {
    ftlobject.jobManager.trigger_readSubmit(req);
    pFIL->read(FIL::Request(req, eventReadDone));
  }
  else {
    // Error while translation
    completeRequest(req);
  }
}

void PageLevelFTL::read_done(uint64_t tag) {
  auto req = getRequest(tag);

  ftlobject.jobManager.trigger_readDone(req);

  completeRequest(req);
}

bool PageLevelFTL::write(Request *cmd) {
  CPU::Function fstat;
  CPU::markFunction(fstat);

  if (UNLIKELY(stalledRequestList.size() > 0 ||
               ftlobject.pAllocator->checkForegroundGCThreshold())) {
    // We have stalled request, so push cmd back to end of list
    if (LIKELY(cmd)) {
      stalledRequestList.emplace_back(cmd);
    }

    // As pop-front command should be pushed to front (not back), check here
    if (!ftlobject.pAllocator->checkForegroundGCThreshold()) {
      bool print = false;
      if (UNLIKELY(cmd == nullptr)) {
        print = true;
      }

      // We will not stall now, so pop cmd from front and handle it
      cmd = stalledRequestList.front();
      stalledRequestList.pop_front();

      if (print) {
        debugprint(Log::DebugID::FTL_PageLevel,
                   "WRITE | LPN %" PRIx64 "h | Resume | Tag %" PRIu64,
                   cmd->getLPN(), cmd->getTag());
      }
    }
    else {
      // if SSD is running out of free block, stall request.
      // Stalled requests will be continued after GC
      if (LIKELY(cmd)) {
        debugprint(Log::DebugID::FTL_PageLevel,
                   "WRITE | LPN %" PRIx64 "h | Stopped by GC | Tag %" PRIu64,
                   cmd->getLPN(), cmd->getTag());
      }

      return false;
    }
  }

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
    // Partial write within page?
    uint32_t skipFront = getSkipFront();
    uint32_t skipEnd = getSkipEnd();

    // Not aligned to minMappingSize
    if (alignedBegin != chunkBegin || alignedEnd != chunkEnd ||
        skipFront != 0 || skipEnd != 0) {
      debugprint(Log::DebugID::FTL_PageLevel,
                 "RMW | INSERT | REQUEST %" PRIx64 "h (+%u) - %" PRIx64
                 "h (-%u) | ALIGN %" PRIx64 "h - %" PRIx64 "h",
                 chunkBegin, skipFront, chunkEnd, skipEnd, alignedBegin,
                 alignedEnd);

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

      ftlobject.jobManager.trigger_writeMapping(ret.front());

      // No need for loop
      ftlobject.pMapping->writeMapping(ret.front(), eventWriteSubmit, false);
    }

    pendingList = std::vector<Request *>(minMappingSize, nullptr);
  }

  scheduleFunction(CPU::CPUGroup::FlashTranslationLayer, InvalidEventID, fstat);

  return true;
}

void PageLevelFTL::write_submit(uint64_t tag) {
  auto list = getWriteContext(tag);

  auto req = list->front();
  auto lpn = req->getLPN();
  auto ppn = req->getPPN();
  uint32_t offset = 0;

  ftlobject.jobManager.trigger_writeSubmit(req);

  for (auto &req : *list) {
    if (LIKELY(req->getResponse() == Response::Success)) {
      pFIL->program(FIL::Request(
          static_cast<LPN>(lpn + offset), static_cast<PPN>(ppn + offset),
          req->getDRAMAddress(), eventWriteDone, req->getTag()));
    }
    else {
      completeRequest(req);
    }

    offset++;
  }

  writeList.erase(list);
}

void PageLevelFTL::write_done(uint64_t tag) {
  auto req = getRequest(tag);

  ftlobject.jobManager.trigger_writeDone(req);

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
    LPN lpnBegin = static_cast<LPN>(cmd->getLPN() - diff);
    PPN ppnBegin = static_cast<PPN>(cmd->getPPN() - diff);
    uint64_t offset = 0;

    for (auto &cmd : ctx.list) {
      if (!cmd || cmd->getOffset() != 0 || cmd->getLength() != pageSize) {
        uint64_t memaddr = cmd ? cmd->getDRAMAddress()
                               : pendingListBaseAddress + offset * pageSize;

        pFIL->read(FIL::Request(lpnBegin, ppnBegin, memaddr,
                                eventPartialReadDone, tag));

        ctx.counter++;
      }

      ++ppnBegin;
      ++lpnBegin;
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
    ftlobject.pMapping->writeMapping(cmd, eventPartialWriteSubmit, false);
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
    LPN lpnBegin = static_cast<LPN>(cmd->getLPN() - diff);
    PPN ppnBegin = static_cast<PPN>(cmd->getPPN() - diff);
    uint64_t offset = 0;

    for (auto &cmd : ctx.list) {
      if (cmd) {
        pFIL->program(FIL::Request(lpnBegin, ppnBegin, cmd->getDRAMAddress(),
                                   eventPartialWriteDone, tag));
      }
      else {
        pFIL->program(FIL::Request(lpnBegin, ppnBegin,
                                   pendingListBaseAddress + offset * pageSize,
                                   eventPartialWriteDone, tag));
      }

      ctx.counter++;

      ++ppnBegin;
      ++lpnBegin;
      offset++;
    }

    stat.rmwWrittenPages += ctx.counter;
  }
  else {
    ctx.counter = 1;

    rmw_writeDone(now, tag);
  }
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

void PageLevelFTL::restartStalledRequests() {
  while (!stalledRequestList.empty()) {
    if (!write(nullptr)) {
      break;
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

  backupSuperRequest(out, pendingList);

  BACKUP_STL(out, writeList, iter, { backupSuperRequest(out, iter); });

  BACKUP_STL(out, rmwList, iter, {
    BACKUP_SCALAR(out, iter.first);

    iter.second.createCheckpoint(out);

    exist = iter.second.next != nullptr;
    auto next = iter.second.next;

    while (true) {
      BACKUP_SCALAR(out, exist);

      if (exist) {
        next->createCheckpoint(out);

        next = next->next;
        exist = next != nullptr;
      }
      else {
        break;
      }
    }
  });

  BACKUP_STL(out, stalledRequestList, iter, {
    auto tag = iter->getTag();

    BACKUP_SCALAR(out, tag);
  });

  BACKUP_SCALAR(out, stat);
  BACKUP_EVENT(out, eventReadSubmit);
  BACKUP_EVENT(out, eventReadDone);
  BACKUP_EVENT(out, eventWriteSubmit);
  BACKUP_EVENT(out, eventWriteDone);
  BACKUP_EVENT(out, eventPartialReadSubmit);
  BACKUP_EVENT(out, eventPartialReadDone);
  BACKUP_EVENT(out, eventPartialWriteSubmit);
  BACKUP_EVENT(out, eventPartialWriteDone);
  BACKUP_EVENT(out, eventInvalidateSubmit);
}

void PageLevelFTL::restoreCheckpoint(std::istream &in) noexcept {
  bool exist;

  restoreSuperRequest(in, pendingList, this);

  RESTORE_STL(in, i, {
    SuperRequest list;

    restoreSuperRequest(in, list, this);

    writeList.emplace_back(std::move(list));
  });

  RESTORE_STL_RESERVE(in, rmwList, i, {
    ReadModifyWriteContext cur;
    uint64_t tag;

    RESTORE_SCALAR(in, tag);

    cur.restoreCheckpoint(in, this);

    while (true) {
      RESTORE_SCALAR(in, exist);

      if (exist) {
        ReadModifyWriteContext *next = new ReadModifyWriteContext();

        next->restoreCheckpoint(in, this);

        cur.push_back(next);
      }
      else {
        break;
      }
    }

    rmwList.emplace(tag, cur);
  });

  RESTORE_STL(in, i, {
    uint64_t tag;

    RESTORE_SCALAR(in, tag);

    stalledRequestList.emplace_back(getRequest(tag));
  });

  RESTORE_SCALAR(in, stat);
  RESTORE_EVENT(in, eventReadSubmit);
  RESTORE_EVENT(in, eventReadDone);
  RESTORE_EVENT(in, eventWriteSubmit);
  RESTORE_EVENT(in, eventWriteDone);
  RESTORE_EVENT(in, eventPartialReadSubmit);
  RESTORE_EVENT(in, eventPartialReadDone);
  RESTORE_EVENT(in, eventPartialWriteSubmit);
  RESTORE_EVENT(in, eventPartialWriteDone);
  RESTORE_EVENT(in, eventInvalidateSubmit);
}

}  // namespace SimpleSSD::FTL
