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
      beginAt(0),
      firstRequestArrival(std::numeric_limits<uint64_t>::max()) {
  logid = Log::DebugID::FTL_NaiveGC;
  state = State::Idle;

  auto pagesInBlock = object.config->getNANDStructure()->page;
  auto param = ftlobject.pMapping->getInfo();

  pageSize = param->pageSize;
  superpage = param->superpage;

  auto sbsize = pagesInBlock * superpage * pageSize;

  // Try SRAM
  if (object.memory->allocate(sbsize, Memory::MemoryType::SRAM, "", true) ==
      0) {
    bufferBaseAddress = object.memory->allocate(
        sbsize, Memory::MemoryType::SRAM, "FTL::GC::Buffer");
  }
  else {
    bufferBaseAddress = object.memory->allocate(
        sbsize, Memory::MemoryType::DRAM, "FTL::GC::Buffer");
  }

  eventTrigger = createEvent([this](uint64_t, uint64_t) { gc_trigger(); },
                             "FTL::GC::eventTrigger");
  eventStart = createEvent([this](uint64_t, uint64_t) { gc_start(); },
                           "FTL::GC::eventStart");
  eventDoRead = createEvent([this](uint64_t t, uint64_t) { gc_doRead(t); },
                            "FTL::GC::eventDoRead");
  eventDoTranslate =
      createEvent([this](uint64_t t, uint64_t) { gc_doTranslate(t); },
                  "FTL::GC::eventDoTranslate");
  eventDoWrite = createEvent([this](uint64_t t, uint64_t) { gc_doWrite(t); },
                             "FTL::GC::eventDoWrite");
  eventWriteDone =
      createEvent([this](uint64_t t, uint64_t) { gc_writeDone(t); },
                  "FTL::GC::eventWriteDone");
  eventDone = createEvent([this](uint64_t t, uint64_t) { gc_done(t); },
                          "FTL::GC::eventEraseDone");

  resetStatValues();
}

NaiveGC::~NaiveGC() {}

void NaiveGC::initialize() {
  AbstractGC::initialize();
}

void NaiveGC::triggerForeground() {
  if (UNLIKELY(ftlobject.pAllocator->checkForegroundGCThreshold() &&
               state == State::Idle)) {
    state = State::Foreground;
    beginAt = getTick();

    scheduleNow(eventTrigger);
  }
}

void NaiveGC::requestArrived(Request *) {
  // Save tick for penalty calculation
  if (UNLIKELY(state >= State::Foreground)) {
    // GC in-progress
    firstRequestArrival = MIN(firstRequestArrival, getTick());
    stat.affected_requests++;
  }
}

bool NaiveGC::checkWriteStall() {
  return ftlobject.pAllocator->checkWriteStall();
}

void NaiveGC::gc_trigger() {
  stat.fgcCount++;

  // Get blocks to erase
  ftlobject.pAllocator->getVictimBlocks(targetBlock, eventStart);

  debugprint(logid, "GC    | Foreground");
}

void NaiveGC::gc_start() {
  // Request copy context
  ftlobject.pMapping->getCopyContext(targetBlock, eventDoRead);
}

void NaiveGC::gc_doRead(uint64_t now) {
  LPN invlpn;

  if (LIKELY(targetBlock.pageReadIndex < targetBlock.copyList.size())) {
    // Do read
    auto &ctx = targetBlock.copyList.at(targetBlock.pageReadIndex++);
    auto ppn = param->makePPN(targetBlock.blockID, 0, ctx.pageIndex);

    if (superpage > 1) {
      debugprint(logid, "GC    | READ  | PSBN %" PRIx64 "h | PSPN %" PRIx64 "h",
                 targetBlock.blockID, param->getPSPNFromPPN(ppn));
    }
    else {
      debugprint(logid, "GC    | READ  | PBN %" PRIx64 "h | PPN %" PRIx64 "h",
                 targetBlock.blockID, ppn);
    }

    for (uint32_t i = 0; i < superpage; i++) {
      ppn = param->makePPN(targetBlock.blockID, i, ctx.pageIndex);

      // Fill request
      if (i == 0) {
        ctx.request.setPPN(ppn);
        ctx.request.setDRAMAddress(makeBufferAddress(i, ctx.pageIndex));

        pFIL->read(FIL::Request(&ctx.request, eventDoTranslate));
      }
      else {
        pFIL->read(FIL::Request(invlpn, ppn,
                                makeBufferAddress(i, ctx.pageIndex),
                                eventDoTranslate, 0ul));
      }
    }

    targetBlock.readCounter = superpage;
    ctx.beginAt = now;
    stat.gcCopiedPages += superpage;
  }
  else {
    // Do erase
    PSBN psbn = targetBlock.blockID;

    // Erase
    if (superpage > 1) {
      debugprint(logid, "GC    | ERASE | PSBN %" PRIx64 "h", psbn);
    }
    else {
      // PSBN == PBN when superpage == 1
      debugprint(logid, "GC    | ERASE | PBN %" PRIx64 "h", psbn);
    }

    for (uint32_t i = 0; i < superpage; i++) {
      pFIL->erase(FIL::Request(invlpn, param->makePPN(psbn, i, 0), 0,
                               eventEraseDone, 0ul));
    }

    targetBlock.beginAt = now;
    targetBlock.writeCounter = superpage;  // Reuse
    stat.gcErasedBlocks += superpage;
  }
}

void NaiveGC::gc_doTranslate(uint64_t now) {
  targetBlock.readCounter--;

  if (targetBlock.readCounter == 0) {
    // Start address translation
    auto &ctx = targetBlock.copyList.at(targetBlock.pageWriteIndex);
    auto lpn = ctx.request.getLPN();
    auto ppn = ctx.request.getPPN();

    panic_if(!lpn.isValid(), "Invalid LPN received.");

    if (superpage > 1) {
      debugprint(
          logid,
          "GC    | READ  | PSBN %" PRIx64 "h | PSPN %" PRIx64
          "h -> LSPN %" PRIx64 "h | %" PRIu64 " - %" PRIu64 " (%" PRIu64 ")",
          targetBlock.blockID, param->getPSPNFromPPN(ppn),
          param->getLSPNFromLPN(lpn), ctx.beginAt, now, now - ctx.beginAt);
    }
    else {
      debugprint(
          logid,
          "GC    | READ  | PBN %" PRIx64 "h | PPN %" PRIx64 "h -> LPN %" PRIx64
          "h | %" PRIu64 " - %" PRIu64 " (%" PRIu64 ")",
          targetBlock.blockID, ppn, lpn, ctx.beginAt, now, now - ctx.beginAt);
    }

    ftlobject.pMapping->writeMapping(&ctx.request, eventDoWrite);
  }
}

void NaiveGC::gc_doWrite(uint64_t now) {
  auto &ctx = targetBlock.copyList.at(targetBlock.pageWriteIndex++);
  auto lpn = ctx.request.getLPN();
  auto ppn = ctx.request.getPPN();

  if (superpage > 1) {
    debugprint(logid,
               "GC    | WRITE | PSBN %" PRIx64 "h | LSPN %" PRIx64
               "h -> PSPN %" PRIx64 "h",
               targetBlock.blockID, param->getLSPNFromLPN(lpn),
               param->getPSPNFromPPN(ppn));
  }
  else {
    debugprint(logid,
               "GC    | WRITE | PBN %" PRIx64 "h | LPN %" PRIx64
               "h -> PPN %" PRIx64 "h",
               targetBlock.blockID, lpn, ppn);
  }

  for (uint32_t i = 0; i < superpage; i++) {
    pFIL->program(
        FIL::Request(static_cast<LPN>(lpn + i), static_cast<PPN>(ppn + i),
                     makeBufferAddress(i, ctx.pageIndex), eventWriteDone, 0ul));
  }

  targetBlock.writeCounter += superpage;  // Do not overwrite
  ctx.beginAt = now;
}

void NaiveGC::gc_writeDone(uint64_t now) {
  targetBlock.writeCounter--;

  if (targetBlock.writeCounter == 0) {
    auto &ctx = targetBlock.copyList.at(targetBlock.pageWriteIndex - 1);
    auto lpn = ctx.request.getLPN();
    auto ppn = ctx.request.getPPN();

    if (superpage > 1) {
      debugprint(
          logid,
          "GC    | WRITE | PSBN %" PRIx64 "h | LSPN %" PRIx64
          "h -> PSPN %" PRIx64 "h | %" PRIu64 " - %" PRIu64 " (%" PRIu64 ")",
          targetBlock.blockID, param->getLSPNFromLPN(lpn),
          param->getPSPNFromPPN(ppn), ctx.beginAt, now, now - ctx.beginAt);
    }
    else {
      debugprint(
          logid,
          "GC    | WRITE | PBN %" PRIx64 "h | LPN %" PRIx64 "h -> PPN %" PRIx64
          "h | %" PRIu64 " - %" PRIu64 " (%" PRIu64 ")",
          targetBlock.blockID, lpn, ppn, ctx.beginAt, now, now - ctx.beginAt);
    }

    // Go back to read
    scheduleNow(eventDoRead);
  }
}

void NaiveGC::gc_eraseDone(uint64_t now) {
  targetBlock.writeCounter--;

  if (targetBlock.writeCounter == 0) {
    PSBN psbn = targetBlock.blockID;

    // Erase completed
    if (superpage > 1) {
      debugprint(logid,
                 "GC    | ERASE | PSBN %" PRIx64 "h | %" PRIu64 " - %" PRIu64
                 " (%" PRIu64 ")",
                 psbn, targetBlock.beginAt, now, now - targetBlock.beginAt);
    }
    else {
      debugprint(logid,
                 "GC    | ERASE | PBN %" PRIx64 "h | %" PRIu64 " - %" PRIu64
                 " (%" PRIu64 ")",
                 psbn, targetBlock.beginAt, now, now - targetBlock.beginAt);
    }

    // Mark table/block as erased
    ftlobject.pMapping->markBlockErased(psbn);
    ftlobject.pAllocator->reclaimBlocks(psbn, eventDone);
  }
}

void NaiveGC::gc_done(uint64_t now) {
  gc_checkDone(now);

  // Calculate penalty
  updatePenalty(now);

  if (state == State::Idle) {
    // Not triggered
    ftlobject.pFTL->restartStalledRequests();
  }
}

void NaiveGC::gc_checkDone(uint64_t now) {
  // Triggered GC completed
  debugprint(logid,
             "GC    | Foreground | %" PRIu64 " - %" PRIu64 " (%" PRIu64 ")",
             beginAt, now, now - beginAt);

  targetBlock.blockID.invalidate();
  state = State::Idle;
  firstRequestArrival = std::numeric_limits<uint64_t>::max();

  // Check threshold
  triggerForeground();
}

void NaiveGC::getStatList(std::vector<Stat> &list,
                          std::string prefix) noexcept {
  list.emplace_back(prefix + "foreground", "Total Foreground GC count");
  list.emplace_back(prefix + "background", "Total Background GC count");
  list.emplace_back(prefix + "block", "Total reclaimed blocks in GC");
  list.emplace_back(prefix + "copy", "Total valid page copy");
  list.emplace_back(prefix + "penalty.average", "Averagy penalty / GC");
  list.emplace_back(prefix + "penalty.min", "Minimum penalty");
  list.emplace_back(prefix + "penalty.max", "Maximum penalty");
  list.emplace_back(prefix + "penalty.count", "# penalty calculation");
}

void NaiveGC::getStatValues(std::vector<double> &values) noexcept {
  auto copy = stat;

  if (firstRequestArrival != std::numeric_limits<uint64_t>::max()) {
    // On-going GC
    auto penalty = getTick() - firstRequestArrival;

    copy.penalty_count++;
    copy.avg_penalty += penalty;
    copy.min_penalty = MIN(copy.min_penalty, penalty);
    copy.max_penalty = MAX(copy.max_penalty, penalty);
  }

  values.push_back((double)copy.fgcCount);
  values.push_back((double)copy.bgcCount);
  values.push_back((double)copy.gcErasedBlocks);
  values.push_back((double)copy.gcCopiedPages);
  values.push_back(copy.penalty_count > 0
                       ? (double)copy.avg_penalty / copy.penalty_count
                       : 0.);
  values.push_back((double)(copy.penalty_count > 0 ? copy.min_penalty : 0));
  values.push_back((double)copy.max_penalty);
  values.push_back((double)copy.penalty_count);
}

void NaiveGC::resetStatValues() noexcept {
  memset(&stat, 0, sizeof(stat));

  stat.min_penalty = std::numeric_limits<uint64_t>::max();
}

void NaiveGC::createCheckpoint(std::ostream &out) const noexcept {
  AbstractGC::createCheckpoint(out);

  targetBlock.createCheckpoint(out);

  BACKUP_SCALAR(out, stat);
  BACKUP_SCALAR(out, firstRequestArrival);

  BACKUP_EVENT(out, eventTrigger);
  BACKUP_EVENT(out, eventStart);
  BACKUP_EVENT(out, eventDoRead);
  BACKUP_EVENT(out, eventDoTranslate);
  BACKUP_EVENT(out, eventDoWrite);
  BACKUP_EVENT(out, eventWriteDone);
  BACKUP_EVENT(out, eventEraseDone);
  BACKUP_EVENT(out, eventDone);
}

void NaiveGC::restoreCheckpoint(std::istream &in) noexcept {
  AbstractGC::restoreCheckpoint(in);

  targetBlock.restoreCheckpoint(in);

  RESTORE_SCALAR(in, stat);
  RESTORE_SCALAR(in, firstRequestArrival);

  RESTORE_EVENT(in, eventTrigger);
  RESTORE_EVENT(in, eventStart);
  RESTORE_EVENT(in, eventDoRead);
  RESTORE_EVENT(in, eventDoTranslate);
  RESTORE_EVENT(in, eventDoWrite);
  RESTORE_EVENT(in, eventWriteDone);
  RESTORE_EVENT(in, eventEraseDone);
  RESTORE_EVENT(in, eventDone);
}

}  // namespace SimpleSSD::FTL::GC
