// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 *         Junhyeok Jang <jhjang@camelab.org>
 */

#include "ftl/base/basic_ftl.hh"

namespace SimpleSSD::FTL {

BasicFTL::BasicFTL(ObjectData &o, FTL *p, FIL::FIL *f,
                   Mapping::AbstractMapping *m,
                   BlockAllocator::AbstractAllocator *a)
    : AbstractFTL(o, p, f, m, a) {
  memset(&stat, 0, sizeof(stat));

  auto param = pMapper->getInfo();
  pageSize = param->pageSize;
  uint64_t pagesInBlock = param->block;

  pMapper->getMappingSize(&minMappingSize);

  pendingList = std::vector<Request *>(minMappingSize, nullptr);

  pendingListBaseAddress = object.memory->allocate(
      minMappingSize * pageSize, Memory::MemoryType::DRAM,
      "FTL::BasicFTL::PendingRMWData");
  gcctx.bufferBaseAddress = object.memory->allocate(
      pagesInBlock * minMappingSize * pageSize, Memory::MemoryType::DRAM,
      "FTL::BasicFTL::GCBuffer");

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
      createEvent([this](uint64_t t, uint64_t d) { rmw_readSubmit(t, d); },
                  "FTL::BasicFTL::eventPartialReadSubmit");
  eventPartialReadDone =
      createEvent([this](uint64_t t, uint64_t d) { rmw_readDone(t, d); },
                  "FTL::BasicFTL::eventPartialReadDone");
  eventPartialWriteSubmit =
      createEvent([this](uint64_t t, uint64_t d) { rmw_writeSubmit(t, d); },
                  "FTL::BasicFTL::eventPartialWriteSubmit");
  eventPartialWriteDone =
      createEvent([this](uint64_t t, uint64_t d) { rmw_writeDone(t, d); },
                  "FTL::BasicFTL::eventPartialWriteDone");

  eventInvalidateSubmit =
      createEvent([this](uint64_t t, uint64_t d) { invalidate_submit(t, d); },
                  "FTL::BasicFTL::eventInvalidateSubmit");

  eventGCTrigger = createEvent([this](uint64_t t, uint64_t) { gc_trigger(t); },
                               "FTL::BasicFTL::eventGCTrigger");

  eventGCSetNextVictimBlock =
      createEvent([this](uint64_t t, uint64_t) { gc_setNextVictimBlock(t); },
                  "FTL::BasicFTL::eventGCSetNextVictimBlock");

  eventGCReadSubmit =
      createEvent([this](uint64_t, uint64_t) { gc_readSubmit(); },
                  "FTL::BasicFTL::eventGCReadSubmit");

  eventGCReadDone =
      createEvent([this](uint64_t t, uint64_t) { gc_readDone(t); },
                  "FTL::BasicFTL::eventGCReadDone");

  eventGCWriteSubmit =
      createEvent([this](uint64_t, uint64_t d) { gc_writeSubmit(d); },
                  "FTL::BasicFTL::eventGCWriteSubmit");

  eventGCWriteDone =
      createEvent([this](uint64_t t, uint64_t d) { gc_writeDone(t, d); },
                  "FTL::BasicFTL::eventGCWriteDone");

  eventGCEraseSubmit =
      createEvent([this](uint64_t, uint64_t) { gc_eraseSubmit(); },
                  "FTL::BasicFTL::eventGCEraseSubmit");

  eventGCEraseDone =
      createEvent([this](uint64_t t, uint64_t) { gc_eraseDone(t); },
                  "FTL::BasicFTL::eventGCEraseDone");

  eventGCDone = createEvent([this](uint64_t t, uint64_t) { gc_done(t); },
                            "FTL::BasicFTL::eventGCEraseDone");

  mergeReadModifyWrite = readConfigBoolean(Section::FlashTranslation,
                                           Config::Key::MergeReadModifyWrite);
}

BasicFTL::~BasicFTL() {}

std::list<SuperRequest>::iterator BasicFTL::getWriteContext(uint64_t tag) {
  auto iter = writeList.begin();

  for (; iter != writeList.end(); iter++) {
    if (iter->front()->getTag() == tag) {
      break;
    }
  }

  panic_if(iter == writeList.end(), "Unexpected write context.");

  return iter;
}

std::unordered_map<uint64_t, BasicFTL::ReadModifyWriteContext>::iterator
BasicFTL::getRMWContext(uint64_t tag) {
  auto iter = rmwList.find(tag);

  panic_if(iter == rmwList.end(), "Unexpected tag in read-modify-write.");

  return iter;
}

void BasicFTL::read(Request *cmd) {
  pMapper->readMapping(cmd, eventReadSubmit);
}

void BasicFTL::read_submit(uint64_t tag) {
  auto req = getRequest(tag);

  if (LIKELY(req->getResponse() == Response::Success)) {
    pFIL->read(FIL::Request(req, eventReadDone));
  }
  else {
    // Error while translation
    completeRequest(req);
  }
}

void BasicFTL::read_done(uint64_t tag) {
  auto req = getRequest(tag);

  completeRequest(req);
}

void BasicFTL::write(Request *cmd) {
  // if SSD is running out of free block, stall request.
  // Stalled requests will be continued after GC
  if (!pAllocator->checkFreeBlockExist()) {
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
  LPN alignedEnd = alignedBegin + minMappingSize;

  LPN chunkBegin = MAX(slpn, alignedBegin);
  LPN chunkEnd = MIN(slpn + nlp, alignedEnd);

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
        pMapper->readMapping(firstreq, eventPartialReadSubmit);

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
      pMapper->writeMapping(ret.front(), eventWriteSubmit);
    }

    pendingList = std::vector<Request *>(minMappingSize, nullptr);
  }

  scheduleFunction(CPU::CPUGroup::FlashTranslationLayer, InvalidEventID, fstat);
}

void BasicFTL::write_submit(uint64_t tag) {
  auto list = getWriteContext(tag);

  PPN ppn = list->front()->getPPN();
  PPN offset = 0;

  for (auto &req : *list) {
    if (LIKELY(req->getResponse() == Response::Success)) {
      pFIL->program(FIL::Request(ppn + offset, req->getDRAMAddress(),
                                 eventWriteDone, req->getTag()));
    }
    else {
      completeRequest(req);
    }

    offset++;
  }

  writeList.erase(list);

  triggerGC();
}

void BasicFTL::write_done(uint64_t tag) {
  auto req = getRequest(tag);

  completeRequest(req);
}

void BasicFTL::rmw_readSubmit(uint64_t now, uint64_t tag) {
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
    PPN ppnBegin = cmd->getPPN() - diff;
    uint64_t offset = 0;

    for (auto &cmd : ctx.list) {
      if (!cmd) {
        pFIL->read(FIL::Request(ppnBegin,
                                pendingListBaseAddress + offset * pageSize,
                                eventPartialReadDone, tag));

        ctx.counter++;
      }

      ppnBegin++;
      offset++;
    }

    stat.rmwReadPages += ctx.counter;
  }
  else {
    ctx.counter = 1;

    rmw_readDone(now, tag);
  }
}

void BasicFTL::rmw_readDone(uint64_t now, uint64_t tag) {
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
    pMapper->writeMapping(cmd, eventPartialWriteSubmit);
  }
}

void BasicFTL::rmw_writeSubmit(uint64_t now, uint64_t tag) {
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
    PPN ppnBegin = cmd->getPPN() - diff;
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

      ppnBegin++;
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

void BasicFTL::rmw_writeDone(uint64_t now, uint64_t tag) {
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

void BasicFTL::invalidate(Request *cmd) {
  pMapper->invalidateMapping(cmd, eventInvalidateSubmit);
}

void BasicFTL::invalidate_submit(uint64_t, uint64_t tag) {
  auto req = getRequest(tag);

  warn("Trim and Format not implemented.");

  completeRequest(req);
}

void BasicFTL::gc_trigger(uint64_t now) {
  gcctx.init(now);

  stat.gcCount++;

  // victim block selection
  pAllocator->getVictimBlocks(gcctx.victimSBlockList,
                              eventGCSetNextVictimBlock);

  debugprint(Log::DebugID::FTL_PageLevel, "GC    | On-demand | %u blocks",
             gcctx.victimSBlockList.size());
}

void BasicFTL::gc_setNextVictimBlock(uint64_t now) {
  if (LIKELY(gcctx.victimSBlockList.size() > 0)) {
    PPN nextVictimBlock = gcctx.victimSBlockList.back();
    gcctx.victimSBlockList.pop_back();

    debugprint(Log::DebugID::FTL_PageLevel,
               "GC    | Victim BlockID  %" PRIu64 "", nextVictimBlock);
    gcctx.copyctx.sblockID = nextVictimBlock;
    gcctx.copyctx.beginAt = now;
    pMapper->getCopyContext(gcctx.copyctx, eventGCReadSubmit);
  }
  else {
    // no need to perform GC or GC finished
    scheduleNow(eventGCDone);
  }
}

void BasicFTL::gc_readSubmit() {
  // alias
  auto &copyctx = gcctx.copyctx;

  // current victim block has no valid pages to copy
  if (UNLIKELY(copyctx.list.size() == 0))
    scheduleNow(eventGCEraseSubmit);

  if (LIKELY(!copyctx.isReadSubmitDone())) {
    auto firstReq = copyctx.iter->front();
    PPN ppnBegin = firstReq->getPPN();
    uint64_t listIndex = copyctx.iter - copyctx.list.begin();
    uint64_t spBufferBaseAddr =
        gcctx.bufferBaseAddress + listIndex * minMappingSize * pageSize;
    copyctx.readCounter = 0;

    // generate tag per SuperRequest
    uint64_t tag = generateFTLTag();
    copyctx.tag2ListIdx.emplace(tag, listIndex);

    debugprint(Log::DebugID::FTL_PageLevel,
               "GC | READ      | PPN %" PRIu64 " - %" PRIu64, ppnBegin,
               ppnBegin + minMappingSize);

    // submit requests in current SuperRequest
    for (auto req : *copyctx.iter) {
      req->setTag(tag);
      pFIL->read(FIL::Request(req, eventGCReadDone));
      object.memory->write(
          spBufferBaseAddr + (req->getPPN() - ppnBegin) * pageSize, pageSize,
          InvalidEventID, false);
      copyctx.readCounter++;
    }
  }
  else {
    // we did all read we need
  }
}

void BasicFTL::gc_readDone(uint64_t now) {
  auto &copyctx = gcctx.copyctx;
  copyctx.readCounter--;

  if (copyctx.isReadDone()) {
    LPN ppnBegin = copyctx.iter->front()->getPPN();
    debugprint(Log::DebugID::FTL_PageLevel,
               "GC | READDONE  | PPN %" PRIu64 " - %" PRIu64 " | %" PRIu64
               " - %" PRIu64 " (%" PRIu64 ")",
               ppnBegin, ppnBegin + minMappingSize, copyctx.beginAt, now,
               now - copyctx.beginAt);

    // Get first command
    auto req = copyctx.iter->at(0);

    // Write translation
    pMapper->writeMapping(req, eventGCWriteSubmit);

    // submit next copy
    // maybe the timing makes no sense
    copyctx.iter++;
    scheduleNow(eventGCReadSubmit);
  }
  return;
}

void BasicFTL::gc_writeSubmit(uint64_t tag) {
  // alias
  auto &copyctx = gcctx.copyctx;

  const uint64_t listIndex = copyctx.tag2ListIdx.at(tag);
  auto sReq = copyctx.list.at(listIndex);

  auto firstReq = sReq.front();
  LPN lpnBegin = firstReq->getLPN();
  PPN ppnBegin = firstReq->getPPN();
  uint64_t spBufferBaseAddr =
      gcctx.bufferBaseAddress + listIndex * minMappingSize * pageSize;
  uint64_t offset = 0;

  debugprint(Log::DebugID::FTL_PageLevel,
             "GC | WRITE     | LPN %" PRIu64 " - %" PRIu64, lpnBegin,
             lpnBegin + minMappingSize);

  for (auto req : sReq) {
    // update to new PPN
    req->setPPN(ppnBegin + offset);

    // submit
    object.memory->read(spBufferBaseAddr + offset * pageSize, pageSize,
                        InvalidEventID, false);
    pFIL->program(FIL::Request(req, eventGCWriteDone));
    copyctx.writeCounter.at(listIndex)++;
    offset++;
  }
}

void BasicFTL::gc_writeDone(uint64_t now, uint64_t tag) {
  auto &copyctx = gcctx.copyctx;
  const auto listIndex = copyctx.tag2ListIdx.at(tag);
  copyctx.writeCounter.at(listIndex)--;
  stat.gcCopiedPages++;

  if (copyctx.iswriteDone(listIndex)) {
    LPN lpnBegin = copyctx.list.at(listIndex).front()->getLPN();

    copyctx.copyCounter--;
    debugprint(Log::DebugID::FTL_PageLevel,
               "GC | WRITEDONE | LPN %" PRIu64 " - %" PRIu64 " | %" PRIu64
               " - %" PRIu64 " (%" PRIu64 ")",
               lpnBegin, lpnBegin + minMappingSize, copyctx.beginAt, now,
               now - copyctx.beginAt);

    if (copyctx.isCopyDone()) {
      // valid page copy done
      debugprint(Log::DebugID::FTL_PageLevel,
                 "GC | COPYDONE  | BLOCK % " PRIu64 " PAGES %" PRIu64
                 " | %" PRIu64 " - %" PRIu64 " (%" PRIu64 ")",
                 copyctx.sblockID, copyctx.list.size(), copyctx.beginAt, now,
                 now - copyctx.beginAt);
      scheduleNow(eventGCEraseSubmit);
    }
  }
}

void BasicFTL::gc_eraseSubmit() {
  PPN blockId = gcctx.copyctx.sblockID;
  panic_if(pMapper->getValidPages(blockId) > 0, "valid page copy not done");

  debugprint(Log::DebugID::FTL_PageLevel, "GC | ERASE     | BLOCK %" PRIu64 "",
             blockId);
  for (uint i = 0; i < minMappingSize; i++) {
    pFIL->erase(FIL::Request(pMapper->getBlockFromSuperblock(blockId, i),
                             eventGCEraseDone, 0));
    gcctx.copyctx.eraseCounter++;
  }
}

void BasicFTL::gc_eraseDone(uint64_t now) {
  gcctx.erasedBlocks++;
  stat.gcErasedBlocks++;

  auto &copyctx = gcctx.copyctx;
  copyctx.eraseCounter--;

  if (copyctx.isEraseDone()) {
    debugprint(Log::DebugID::FTL_PageLevel,
               "GC | ERASEDONE | BLOCK %" PRIu64 " | %" PRIu64 " - %" PRIu64
               " (%" PRIu64 ")",
               copyctx.sblockID, copyctx.beginAt, now, now - copyctx.beginAt);

    pMapper->markBlockErased(copyctx.sblockID);
    pAllocator->reclaimBlocks(copyctx.sblockID, eventGCSetNextVictimBlock);
  }
}

void BasicFTL::gc_done(uint64_t now) {
  gcctx.inProgress = false;
  debugprint(Log::DebugID::FTL_PageLevel,
             "GC | DONE      | %" PRIu64 " BLOCKS | %" PRIu64 " - %" PRIu64
             " (%" PRIu64 ")",
             gcctx.beginAt, gcctx.erasedBlocks, now, gcctx.beginAt,
             now - gcctx.beginAt);

  // continue stalled request
  while (!stalledRequests.empty()) {
    auto cmd = stalledRequests.front();
    debugprint(Log::DebugID::FTL_PageLevel, "WRITE | CONTINUE | TAG : %" PRIu64,
               cmd->getTag());
    write(cmd);
    stalledRequests.pop_front();

    // if gc restarted, stop continuing
    if (gcctx.inProgress)
      break;
  }
}

void BasicFTL::backup(std::ostream &out, const SuperRequest &list) const
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

void BasicFTL::restore(std::istream &in, SuperRequest &list) noexcept {
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
  list.emplace_back(prefix + "rmw.count", "Total read-modify-write operations");
  list.emplace_back(prefix + "rmw.merge_count",
                    "Total merged read-modify-write operations");
  list.emplace_back(prefix + "rmw.read_pages",
                    "Total read pages in read-modify-write");
  list.emplace_back(prefix + "rmw.written_pages",
                    "Total written pages in read-modify-write");
  list.emplace_back(prefix + "gc.count", "Total GC count");
  list.emplace_back(prefix + "gc.reclaimed_blocks",
                    "Total reclaimed blocks in GC");
  list.emplace_back(prefix + "gc.page_copies", "Total valid page copy");
}

void BasicFTL::getStatValues(std::vector<double> &values) noexcept {
  values.push_back((double)stat.rmwCount);
  values.push_back((double)stat.rmwMerged);
  values.push_back((double)stat.rmwReadPages);
  values.push_back((double)stat.rmwWrittenPages);
  values.push_back((double)stat.gcCount);
  values.push_back((double)stat.gcErasedBlocks);
  values.push_back((double)stat.gcCopiedPages);
}

void BasicFTL::resetStatValues() noexcept {
  memset(&stat, 0, sizeof(stat));
}

void BasicFTL::createCheckpoint(std::ostream &out) const noexcept {
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
  BACKUP_EVENT(out, eventGCTrigger);
}

void BasicFTL::restoreCheckpoint(std::istream &in) noexcept {
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
  RESTORE_EVENT(in, eventGCTrigger);
}

}  // namespace SimpleSSD::FTL
