// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "ftl/base/basic_ftl.hh"

namespace SimpleSSD::FTL {

BasicFTL::BasicFTL(ObjectData &o, FTL *p, FIL::FIL *f,
                   Mapping::AbstractMapping *m,
                   BlockAllocator::AbstractAllocator *a)
    : AbstractFTL(o, p, f, m, a) {
  memset(&stat, 0, sizeof(stat));

  pageSize = pMapper->getInfo()->pageSize;

  pMapper->getMappingSize(&minMappingSize);

  pendingList = std::vector<Request *>(minMappingSize, nullptr);
  pendingLPN = InvalidLPN;

  pendingListBaseAddress = object.memory->allocate(
      minMappingSize * pageSize, Memory::MemoryType::DRAM,
      "FTL::BasicFTL::PendingRMWData");

  // Create events
  eventReadSubmit =
      createEvent([this](uint64_t, uint64_t d) { read_submit(d); },
                  "FTL::BasicFTL::eventReadSubmit");
  eventReadDone = createEvent([this](uint64_t, uint64_t d) { read_done(d); },
                              "FTL::BasicFTL::eventReadDone");

  eventWriteSubmit =
      createEvent([this](uint64_t, uint64_t d) { write_submit(d); },
                  "FTL::BasicFTL::eventWriteSubmit");
  eventWriteDone = createEvent([this](uint64_t, uint64_t d) { write_done(d); },
                               "FTL::BasicFTL::eventWriteDone");
  eventPartialReadSubmit =
      createEvent([this](uint64_t, uint64_t d) { rmw_readSubmit(d); },
                  "FTL::BasicFTL::eventPartialReadSubmit");
  eventPartialReadDone =
      createEvent([this](uint64_t, uint64_t d) { rmw_readDone(d); },
                  "FTL::BasicFTL::eventPartialReadDone");
  eventPartialWriteSubmit =
      createEvent([this](uint64_t, uint64_t d) { rmw_writeSubmit(d); },
                  "FTL::BasicFTL::eventPartialWriteSubmit");
  eventPartialWriteDone =
      createEvent([this](uint64_t, uint64_t d) { rmw_writeDone(d); },
                  "FTL::BasicFTL::eventPartialWriteDone");
  eventMergedWriteDone =
      createEvent([this](uint64_t, uint64_t d) { rmw_mergeDone(); },
                  "FTL::BasicFTL::eventMergedWriteDone");

  eventInvalidateSubmit =
      createEvent([this](uint64_t t, uint64_t d) { invalidate_submit(t, d); },
                  "FTL::BasicFTL::eventInvalidateSubmit");

  eventGCTrigger = createEvent([this](uint64_t t, uint64_t) { gc_trigger(t); },
                               "FTL::BasicFTL::eventGCTrigger");

  mergeReadModifyWrite = readConfigBoolean(Section::FlashTranslation,
                                           Config::Key::MergeReadModifyWrite);
}

BasicFTL::~BasicFTL() {}

BasicFTL::ReadModifyWriteContext *BasicFTL::getRMWContext(uint64_t tag,
                                                          Request **ppcmd) {
  for (auto &iter : rmwList) {
    for (auto &cmd : iter.list) {
      if (cmd && cmd->getTag() == tag) {
        if (ppcmd) {
          *ppcmd = cmd;
        }

        return &iter;
      }
    }
  }

  panic("Unexpected tag in read-modify-write.");

  return nullptr;
}

void BasicFTL::read(Request *cmd) {
  pMapper->readMapping(cmd, eventReadSubmit);
}

void BasicFTL::read_submit(uint64_t tag) {
  auto req = getRequest(tag);

  pFIL->read(FIL::Request(req->getPPN(), eventReadDone, tag));

  object.memory->write(req->getDRAMAddress(), pageSize, InvalidEventID);
}

void BasicFTL::read_done(uint64_t tag) {
  auto req = getRequest(tag);

  completeRequest(req);
}

void BasicFTL::write(Request *cmd) {
  CPU::Function fstat;
  CPU::markFunction(fstat);

  LPN lpn = cmd->getLPN();
  LPN slpn = cmd->getSLPN();
  uint32_t nlp = cmd->getNLP();

  LPN alignedBegin = getAlignedLPN(lpn);
  LPN alignedEnd = alignedBegin + minMappingSize;

  LPN chunkBegin = MAX(slpn, alignedBegin);
  LPN chunkEnd = MIN(slpn + nlp, alignedEnd);

  // Store to pending list
  pendingList.at(lpn - alignedBegin) = cmd;

  // Check cmd is final of current chunk
  if (lpn == chunkEnd) {
    // Not aligned to minMappingSize
    if (alignedBegin != chunkBegin || alignedEnd != chunkEnd) {
      bool merged = false;

      if (mergeReadModifyWrite) {
        for (auto &iter : rmwList) {
          if (iter.alignedBegin == alignedBegin && !iter.writePending) {
            auto pctx = new ReadModifyWriteContext();

            pctx->alignedBegin = alignedBegin;
            pctx->chunkBegin = chunkBegin;
            pctx->list = std::move(pendingList);
            iter.push_back(pctx);

            merged = true;
          }
        }
      }

      if (!merged) {
        auto ret = rmwList.emplace_back(pendingLPN, ReadModifyWriteContext());

        ret.list = std::move(pendingList);
        ret.alignedBegin = alignedBegin;
        ret.chunkBegin = chunkBegin;

        // Do read translation - no need for loop
        pMapper->readMapping(pendingList.at(chunkBegin - alignedBegin),
                             eventPartialReadSubmit);
      }
    }
    else {
      // No need for loop
      pMapper->writeMapping(pendingList.at(chunkBegin - alignedBegin),
                            eventWriteSubmit);
    }
  }
  else {
    pendingLPN = alignedBegin;
    pendingList = std::vector<Request *>(minMappingSize, nullptr);
  }

  scheduleFunction(CPU::CPUGroup::FlashTranslationLayer, InvalidEventID, fstat);
}

void BasicFTL::write_submit(uint64_t tag) {
  auto req = getRequest(tag);

  pFIL->program(FIL::Request(req->getPPN(), eventWriteDone, tag));

  object.memory->read(req->getDRAMAddress(), pageSize, InvalidEventID);

  triggerGC();
}

void BasicFTL::write_done(uint64_t tag) {
  auto req = getRequest(tag);

  completeRequest(req);
}

void BasicFTL::rmw_readSubmit(uint64_t tag) {
  auto ctx = getRMWContext(tag);

  // Get first command
  auto cmd = ctx->list.at(ctx->chunkBegin - ctx->alignedBegin);

  // Convert SPPN to PPN
  PPN ppnBegin = cmd->getPPN() * minMappingSize;
  uint64_t offset = 0;

  for (auto &cmd : ctx->list) {
    if (cmd) {
      pFIL->read(FIL::Request(ppnBegin, eventPartialReadDone, cmd->getTag()));
      object.memory->write(pendingListBaseAddress + offset * pageSize, pageSize,
                           InvalidEventID);

      ctx->counter++;
    }

    ppnBegin++;
    offset++;
  }
}

void BasicFTL::rmw_readDone(uint64_t tag) {
  auto ctx = getRMWContext(tag);

  ctx->counter--;

  if (ctx->counter == 0) {
    // Get first command
    auto cmd = ctx->list.at(ctx->chunkBegin - ctx->alignedBegin);

    // Write translation
    pMapper->writeMapping(cmd, eventPartialWriteSubmit);
  }
}

void BasicFTL::rmw_writeSubmit(uint64_t tag) {
  auto ctx = getRMWContext(tag);

  ctx->writePending = true;

  // Get first command
  auto cmd = ctx->list.at(ctx->chunkBegin - ctx->alignedBegin);

  // Convert SPPN to PPN
  PPN ppnBegin = cmd->getPPN() * minMappingSize;
  uint64_t offset = 0;

  for (auto &cmd : ctx->list) {
    pFIL->program(FIL::Request(ppnBegin, eventPartialWriteDone, cmd->getTag()));

    if (cmd) {
      object.memory->read(cmd->getDRAMAddress(), pageSize, InvalidEventID);
    }
    else {
      object.memory->read(pendingListBaseAddress + offset * pageSize, pageSize,
                          InvalidEventID);
    }

    ctx->counter++;

    ppnBegin++;
    offset++;
  }
}

void BasicFTL::rmw_writeDone(uint64_t tag) {
  Request *cmd = nullptr;
  auto ctx = getRMWContext(tag, &cmd);

  ctx->counter--;

  scheduleNow(cmd->getEvent(), cmd->getEventData());

  if (ctx->counter == 0) {
    bool call = false;
    auto next = ctx->next;

    while (next) {
      // Completion of all requests
      for (auto cmd : next->list) {
        mergeList.emplace_back(cmd->getEvent(), cmd->getEventData());
      }

      next = next->next;
      delete ctx->next;

      call = true;
    }

    if (call && !isScheduled(eventMergedWriteDone)) {
      scheduleNow(eventMergedWriteDone);
    }
  }
}

void BasicFTL::rmw_mergeDone() {
  if (mergeList.size() > 0) {
    auto iter = mergeList.front();

    scheduleNow(iter.first, iter.second);

    mergeList.pop_front();

    if (mergeList.size() > 0) {
      scheduleNow(eventMergedWriteDone);
    }
  }
}

void BasicFTL::invalidate(Request *cmd) {
  pMapper->invalidateMapping(cmd, eventInvalidateSubmit);
}

void BasicFTL::invalidate_submit(uint64_t, uint64_t tag) {
  auto req = getRequest(tag);

  warn("Trim and Format not implemented.");

  completeRequest(req);
}

void BasicFTL::gc_trigger(uint64_t) {
  panic("GC not implemented.")
}

void BasicFTL::backup(std::ostream &out,
                      const std::vector<Request *> &list) const noexcept {
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

void BasicFTL::restore(std::istream &in,
                       std::vector<Request *> &list) noexcept {
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

void BasicFTL::getStatList(std::vector<Stat> &list,
                           std::string prefix) noexcept {
  list.emplace_back(prefix + "ftl.gc.count", "Total GC count");
  list.emplace_back(prefix + "ftl.gc.reclaimed_blocks",
                    "Total reclaimed blocks in GC");
  list.emplace_back(prefix + "ftl.gc.superpage_copies",
                    "Total valid superpage copy");
  list.emplace_back(prefix + "ftl.gc.page_copies", "Total valid page copy");
}

void BasicFTL::getStatValues(std::vector<double> &values) noexcept {
  values.push_back((double)stat.count);
  values.push_back((double)stat.blocks);
  values.push_back((double)stat.superpages);
  values.push_back((double)stat.pages);
}

void BasicFTL::resetStatValues() noexcept {
  memset(&stat, 0, sizeof(stat));
}

void BasicFTL::createCheckpoint(std::ostream &out) const noexcept {
  bool exist;
  uint64_t size;

  backup(out, pendingList);

  BACKUP_SCALAR(out, pendingLPN);

  size = rmwList.size();
  BACKUP_SCALAR(out, size);

  for (auto &iter : rmwList) {
    BACKUP_SCALAR(out, iter.alignedBegin);
    BACKUP_SCALAR(out, iter.chunkBegin);

    backup(out, iter.list);

    BACKUP_SCALAR(out, iter.writePending);
    BACKUP_SCALAR(out, iter.counter);

    exist = iter.next != nullptr;
    auto next = iter.next;

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

  size = mergeList.size();
  BACKUP_SCALAR(out, size);

  for (auto &iter : mergeList) {
    BACKUP_EVENT(out, iter.first);
    BACKUP_SCALAR(out, iter.second);
  }

  BACKUP_SCALAR(out, stat);
  BACKUP_EVENT(out, eventReadSubmit);
  BACKUP_EVENT(out, eventReadDone);
  BACKUP_EVENT(out, eventWriteSubmit);
  BACKUP_EVENT(out, eventWriteDone);
  BACKUP_EVENT(out, eventInvalidateSubmit);
  BACKUP_EVENT(out, eventGCTrigger);
}

void BasicFTL::restoreCheckpoint(std::istream &in) noexcept {
  bool exist;
  uint64_t size;

  restore(in, pendingList);

  RESTORE_SCALAR(in, pendingLPN);

  RESTORE_SCALAR(in, size);

  ReadModifyWriteContext cur;

  for (uint64_t i = 0; i < size; i++) {
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
  }

  RESTORE_SCALAR(in, size);

  for (uint64_t i = 0; i < size; i++) {
    Event eid;
    uint64_t tag;

    RESTORE_EVENT(in, eid);
    RESTORE_SCALAR(in, tag);

    mergeList.emplace_back(eid, tag);
  }

  RESTORE_SCALAR(in, stat);
  RESTORE_EVENT(in, eventReadSubmit);
  RESTORE_EVENT(in, eventReadDone);
  RESTORE_EVENT(in, eventWriteSubmit);
  RESTORE_EVENT(in, eventWriteDone);
  RESTORE_EVENT(in, eventInvalidateSubmit);
  RESTORE_EVENT(in, eventGCTrigger);
}

}  // namespace SimpleSSD::FTL
