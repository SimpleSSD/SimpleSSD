// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 *         Junhyeok Jang <jhjang@camelab.org>
 */

#include "ftl/gc/naive.hh"

#include "ftl/allocator/abstract_allocator.hh"
#include "ftl/base/abstract_ftl.hh"
#include "ftl/mapping/abstract_mapping.hh"

namespace SimpleSSD::FTL::GC {

NaiveGC::NaiveGC(ObjectData &o, FTLObjectData &fo, FIL::FIL *f)
    : AbstractGC(o, fo, f),
      beginAt(std::numeric_limits<uint64_t>::max()),
      firstRequestArrival(std::numeric_limits<uint64_t>::max()) {
  auto pagesInBlock = object.config->getNANDStructure()->page;
  auto param = ftlobject.pMapping->getInfo();

  pageSize = param->pageSize;
  superpage = param->superpage;

  auto sbsize = pagesInBlock * superpage * pageSize;

  // Try SRAM
  if (object.memory->allocate(sbsize, Memory::MemoryType::SRAM, "", true) ==
      0) {
    bufferBaseAddress = object.memory->allocate(
        sbsize, Memory::MemoryType::SRAM, "FTL::NaiveGC::Buffer");
  }
  else {
    bufferBaseAddress = object.memory->allocate(
        sbsize, Memory::MemoryType::DRAM, "FTL::NaiveGC::Buffer");
  }

  eventTrigger = createEvent([this](uint64_t, uint64_t) { gc_trigger(); },
                             "FTL::GC::eventTrigger");
  eventStart = createEvent([this](uint64_t t, uint64_t) { gc_start(t); },
                           "FTL::GC::eventStart");
  eventDoRead = createEvent([this](uint64_t t, uint64_t d) { gc_doRead(t, d); },
                            "FTL::GC::eventDoRead");
  eventDoTranslate =
      createEvent([this](uint64_t t, uint64_t d) { gc_doTranslate(t, d); },
                  "FTL::GC::eventDoTranslate");
  eventDoWrite =
      createEvent([this](uint64_t t, uint64_t d) { gc_doWrite(t, d); },
                  "FTL::GC::eventDoWrite");
  eventDoErase =
      createEvent([this](uint64_t t, uint64_t d) { gc_doErase(t, d); },
                  "FTL::GC::eventDoErase");
  eventDone = createEvent([this](uint64_t t, uint64_t d) { gc_done(t, d); },
                          "FTL::GC::eventEraseDone");

  resetStatValues();
}

NaiveGC::~NaiveGC() {}

void NaiveGC::initialize() {
  AbstractGC::initialize();
}

void NaiveGC::triggerForeground() {
  if (ftlobject.pAllocator->checkForegroundGCThreshold() &&
      beginAt == std::numeric_limits<uint64_t>::max()) {
    beginAt = getTick();

    scheduleNow(eventTrigger);
  }
}

void NaiveGC::requestArrived(bool, uint32_t) {
  // Naive GC algorithm does not perform background GC. Ignore.
  firstRequestArrival = MIN(firstRequestArrival, getTick());
}

bool NaiveGC::checkWriteStall() {
  return ftlobject.pAllocator->checkForegroundGCThreshold();
}

void NaiveGC::gc_trigger() {
  stat.fgcCount++;

  // Get blocks to erase
  ftlobject.pAllocator->getVictimBlocks(blockList, eventStart);

  debugprint(Log::DebugID::FTL_NaiveGC, "GC    | Foreground | %u (super)blocks",
             blockList.size());
}

void NaiveGC::gc_start(uint64_t now) {
  if (LIKELY(blockList.size() > 0)) {
    // Parallel submission
    int32_t n =
        param->parallelismLevel[0] / param->superpage - getSessionCount();

    panic_if(n < 0, "Copy session list corrupted");

    for (int32_t count = 0; count < n && blockList.size() > 0; count++) {
      // Start session
      auto tag = startCopySession(std::move(blockList.front()));
      blockList.pop_front();

      // Do read
      scheduleNow(eventDoRead, tag);
    }
  }
  else {
    // Triggered GC completed
    debugprint(Log::DebugID::FTL_NaiveGC,
               "GC    | Foreground | %" PRIu64 " - %" PRIu64 " (%" PRIu64 ")",
               beginAt, now, now - beginAt);

    beginAt = std::numeric_limits<uint64_t>::max();

    // Calculate penalty
    if (firstRequestArrival < now) {
      auto penalty = now - firstRequestArrival;

      stat.penalty_count++;
      stat.avg_penalty += penalty;
      stat.min_penalty = MIN(stat.min_penalty, penalty);
      stat.max_penalty = MAX(stat.max_penalty, penalty);

      firstRequestArrival = std::numeric_limits<uint64_t>::max();
    }

    // Check threshold
    triggerForeground();

    if (beginAt == std::numeric_limits<uint64_t>::max()) {
      // Not triggered
      ftlobject.pFTL->restartStalledRequests();
    }
  }
}

void NaiveGC::gc_doRead(uint64_t now, uint64_t tag) {
  auto &block = findCopySession(tag);

  if (LIKELY(block.pageReadIndex < block.copyList.size())) {
    auto &ctx = block.copyList.at(block.pageReadIndex++);
    auto ppn = param->makePPN(block.blockID, 0, ctx.pageIndex);

    if (superpage > 1) {
      debugprint(Log::DebugID::FTL_NaiveGC, "GC | READ  | PSPN %" PRIx64 "h",
                 param->getPSPNFromPPN(ppn));
    }
    else {
      debugprint(Log::DebugID::FTL_NaiveGC, "GC | READ  | PPN %" PRIx64 "h",
                 ppn);
    }

    for (uint32_t i = 0; i < superpage; i++) {
      ppn = param->makePPN(block.blockID, i, ctx.pageIndex);

      // Fill request
      if (i == 0) {
        ctx.request.setPPN(ppn);
        ctx.request.setDRAMAddress(makeBufferAddress(i, ctx.pageIndex));
        ctx.request.setTag(tag);

        pFIL->read(FIL::Request(&ctx.request, eventDoTranslate));
      }
      else {
        pFIL->read(FIL::Request(ppn, makeBufferAddress(i, ctx.pageIndex),
                                eventDoTranslate, tag));
      }
    }

    block.readCounter = superpage;
    ctx.beginAt = now;
    stat.gcCopiedPages += superpage;
  }
}

void NaiveGC::gc_doTranslate(uint64_t now, uint64_t tag) {
  auto &block = findCopySession(tag);

  block.readCounter--;

  if (block.readCounter == 0) {
    // Read completed, start next read
    scheduleNow(eventDoRead, tag);

    // Start address translation
    auto &ctx = block.copyList.at(block.pageWriteIndex);
    auto lpn = ctx.request.getLPN();
    auto ppn = ctx.request.getPPN();

    panic_if(!lpn.isValid(), "Invalid LPN received.");

    if (superpage > 1) {
      debugprint(Log::DebugID::FTL_NaiveGC,
                 "GC | READ  | PSPN %" PRIx64 "h -> LSPN %" PRIx64
                 "h | %" PRIu64 " - %" PRIu64 " (%" PRIu64 ")",
                 param->getPSPNFromPPN(ppn), param->getLSPNFromLPN(lpn),
                 ctx.beginAt, now, now - ctx.beginAt);
    }
    else {
      debugprint(Log::DebugID::FTL_NaiveGC,
                 "GC | READ  | PPN %" PRIx64 "h -> LPN %" PRIx64 "h | %" PRIu64
                 " - %" PRIu64 " (%" PRIu64 ")",
                 ppn, lpn, ctx.beginAt, now, now - ctx.beginAt);
    }

    ftlobject.pMapping->writeMapping(&ctx.request, eventDoWrite);
  }
}

void NaiveGC::gc_doWrite(uint64_t now, uint64_t tag) {
  auto &block = findCopySession(tag);
  auto &ctx = block.copyList.at(block.pageWriteIndex++);
  auto lpn = ctx.request.getLPN();
  auto ppn = ctx.request.getPPN();

  if (superpage > 1) {
    debugprint(Log::DebugID::FTL_NaiveGC,
               "GC | WRITE | LSPN %" PRIx64 "h -> PSPN %" PRIx64 "h",
               param->getLSPNFromLPN(lpn), param->getPSPNFromPPN(ppn));
  }
  else {
    debugprint(Log::DebugID::FTL_NaiveGC,
               "GC | WRITE | LPN %" PRIx64 "h -> PPN %" PRIx64 "h", lpn, ppn);
  }

  for (uint32_t i = 0; i < superpage; i++) {
    pFIL->program(FIL::Request(static_cast<PPN>(ppn + i),
                               makeBufferAddress(i, ctx.pageIndex),
                               eventDoErase, tag));
  }

  block.writeCounter += superpage;  // Do not overwrite
  ctx.beginAt = now;
}

void NaiveGC::gc_doErase(uint64_t now, uint64_t tag) {
  auto &block = findCopySession(tag);

  block.writeCounter--;

  if (block.writeCounter == 0 &&
      block.pageWriteIndex == block.copyList.size()) {
    PSBN psbn = block.blockID;

    // Copy completed
    debugprint(Log::DebugID::FTL_NaiveGC,
               "GC | WRITE | %" PRIu64 " - %" PRIu64 " (%" PRIu64 ")",
               block.copyList.front().beginAt, now,
               now - block.copyList.front().beginAt);

    // Erase
    if (superpage > 1) {
      debugprint(Log::DebugID::FTL_NaiveGC, "GC | ERASE | PSBN %" PRIx64 "h",
                 psbn);
    }
    else {
      debugprint(Log::DebugID::FTL_NaiveGC, "GC | ERASE | PBN %" PRIx64 "h",
                 psbn);  // PSBN == PBN when superpage == 1
    }

    for (uint32_t i = 0; i < superpage; i++) {
      pFIL->erase(FIL::Request(param->makePPN(psbn, i, 0), 0, eventDone, tag));
    }

    block.copyList.back().beginAt = now;
    block.writeCounter = superpage;  // Reuse
    stat.gcErasedBlocks += superpage;
  }
}

void NaiveGC::gc_done(uint64_t now, uint64_t tag) {
  auto &block = findCopySession(tag);

  block.writeCounter--;

  if (block.writeCounter == 0) {
    PSBN psbn = block.blockID;
    uint64_t beginAt = block.copyList.back().beginAt;

    // Erase completed
    if (superpage > 1) {
      debugprint(Log::DebugID::FTL_NaiveGC,
                 "GC | ERASE | PSBN %" PRIx64 "h | %" PRIu64 " - %" PRIu64
                 " (%" PRIu64 ")",
                 psbn, beginAt, now, now - beginAt);
    }
    else {
      debugprint(Log::DebugID::FTL_NaiveGC,
                 "GC | ERASE | PBN %" PRIx64 "h | %" PRIu64 " - %" PRIu64
                 " (%" PRIu64 ")",
                 psbn, beginAt, now, now - beginAt);
    }

    closeCopySession(tag);

    // Mark table/block as erased
    ftlobject.pMapping->markBlockErased(psbn);
    ftlobject.pAllocator->reclaimBlocks(psbn, eventStart);
  }
}

void NaiveGC::getStatList(std::vector<Stat> &list,
                          std::string prefix) noexcept {
  list.emplace_back(prefix + "foreground", "Total Foreground GC count");
  list.emplace_back(prefix + "block", "Total reclaimed blocks in GC");
  list.emplace_back(prefix + "copy", "Total valid page copy");
  list.emplace_back(prefix + "penalty.average", "Averagy penalty / GC");
  list.emplace_back(prefix + "penalty.min", "Minimum penalty");
  list.emplace_back(prefix + "penalty.max", "Maximum penalty");
  list.emplace_back(prefix + "penalty.count", "# penalty calculation");
}

void NaiveGC::getStatValues(std::vector<double> &values) noexcept {
  values.push_back((double)stat.fgcCount);
  values.push_back((double)stat.gcErasedBlocks);
  values.push_back((double)stat.gcCopiedPages);
  values.push_back((double)stat.avg_penalty / stat.penalty_count);
  values.push_back((double)stat.min_penalty);
  values.push_back((double)stat.max_penalty);
  values.push_back((double)stat.penalty_count);
}

void NaiveGC::resetStatValues() noexcept {
  memset(&stat, 0, sizeof(stat));
}

void NaiveGC::createCheckpoint(std::ostream &out) const noexcept {
  AbstractGC::createCheckpoint(out);

  uint64_t size = blockList.size();
  BACKUP_SCALAR(out, size);

  for (auto &iter : blockList) {
    iter.createCheckpoint(out);
  }

  BACKUP_SCALAR(out, stat);
  BACKUP_SCALAR(out, firstRequestArrival);

  BACKUP_EVENT(out, eventTrigger);
  BACKUP_EVENT(out, eventStart);
  BACKUP_EVENT(out, eventDoRead);
  BACKUP_EVENT(out, eventDoTranslate);
  BACKUP_EVENT(out, eventDoWrite);
  BACKUP_EVENT(out, eventDoErase);
  BACKUP_EVENT(out, eventDone);
}

void NaiveGC::restoreCheckpoint(std::istream &in) noexcept {
  AbstractGC::restoreCheckpoint(in);

  uint64_t size;
  RESTORE_SCALAR(in, size);

  for (uint64_t i = 0; i < size; i++) {
    CopyContext ctx;

    ctx.restoreCheckpoint(in);

    blockList.emplace_back(std::move(ctx));
  }

  RESTORE_SCALAR(in, stat);
  RESTORE_SCALAR(in, firstRequestArrival);

  RESTORE_EVENT(in, eventTrigger);
  RESTORE_EVENT(in, eventStart);
  RESTORE_EVENT(in, eventDoRead);
  RESTORE_EVENT(in, eventDoTranslate);
  RESTORE_EVENT(in, eventDoWrite);
  RESTORE_EVENT(in, eventDoErase);
  RESTORE_EVENT(in, eventDone);
}

}  // namespace SimpleSSD::FTL::GC
