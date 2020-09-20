// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "ftl/gc/advanced.hh"

#include "ftl/allocator/abstract_allocator.hh"
#include "ftl/base/abstract_ftl.hh"

namespace SimpleSSD::FTL::GC {

AdvancedGC::AdvancedGC(ObjectData &o, FTLObjectData &fo, FIL::FIL *f)
    : NaiveGC(o, fo, f),
      pendingFIL(0),
      preemptRequestedAt(std::numeric_limits<uint64_t>::max()) {
  logid = Log::DebugID::FTL_AdvancedGC;

  idletime = readConfigUint(Section::FlashTranslation,
                            Config::Key::IdleTimeForBackgroundGC);

  // Create event
  eventBackgroundGC =
      createEvent([this](uint64_t t, uint64_t) { triggerBackground(t); },
                  "FTL::GC::eventBackgroundGC");

  // Schedule
  scheduleRel(eventBackgroundGC, 0, idletime);
}

AdvancedGC::~AdvancedGC() {}

void AdvancedGC::triggerBackground(uint64_t now) {
  if (UNLIKELY(ftlobject.pAllocator->checkBackgroundGCThreshold() &&
               state < State::Foreground)) {
    if (state == State::Paused) {
      resumePaused();
    }
    else {
      scheduleNow(eventTrigger);
    }

    state = State::Background;
    beginAt = now;
  }
}

void AdvancedGC::triggerForeground() {
  if (UNLIKELY(ftlobject.pAllocator->checkForegroundGCThreshold() &&
               state < State::Foreground)) {
    if (state == State::Paused) {
      resumePaused();
    }
    else {
      scheduleNow(eventTrigger);
    }

    state = State::Foreground;
    beginAt = getTick();
  }
}

void AdvancedGC::requestArrived(Request *req) {
  // Penalty calculation & Background GC invocation
  NaiveGC::requestArrived(req);

  // Not scheduled
  if (LIKELY(state < State::Foreground)) {
    // Reschedule GC check
    if (isScheduled(eventBackgroundGC)) {
      deschedule(eventBackgroundGC);
    }

    scheduleRel(eventBackgroundGC, 0, idletime);
  }

  // Request preemption
  if (UNLIKELY(state >= State::Foreground && !preemptRequested())) {
    preemptRequestedAt = getTick();

    debugprint(logid, "GC    | Preemption requested");
  }
}

void AdvancedGC::resumePaused() {
  debugprint(logid, "GC    | Resume from preempted state");

  for (auto &iter : ongoingCopy) {
    auto &block = iter.second;

    panic_if(block.writeCounter != 0 || block.readCounter != 0,
             "Unexpected GC preemption state");

    if (block.pageWriteIndex == block.pageReadIndex) {
      scheduleNow(eventDoRead, iter.first);
    }
    else {
      panic("Unexpected GC preemption state");
    }
  }
}

void AdvancedGC::gc_trigger() {
  // Get blocks to erase
  ftlobject.pAllocator->getVictimBlocks(blockList, eventStart);

  if (ftlobject.pAllocator->checkForegroundGCThreshold()) {
    stat.fgcCount++;
    state = State::Foreground;

    debugprint(logid, "GC    | Foreground | %u (super)blocks",
               blockList.size());
  }
  else if (state == State::Background) {
    stat.bgcCount++;
    state = State::Background;

    debugprint(logid, "GC    | Background | %u (super)blocks",
               blockList.size());
  }
}

void AdvancedGC::gc_checkDone(uint64_t now) {
  // Maybe GC is completed while waiting for pending requests
  checkPreemptible();

  // Triggered GC completed
  if (state == State::Foreground) {
    debugprint(logid,
               "GC    | Foreground | %" PRIu64 " - %" PRIu64 " (%" PRIu64 ")",
               beginAt, now, now - beginAt);
  }
  else if (state == State::Background) {
    debugprint(logid,
               "GC    | Background | %" PRIu64 " - %" PRIu64 " (%" PRIu64 ")",
               beginAt, now, now - beginAt);
  }

  state = State::Idle;

  // Check threshold
  triggerForeground();
}

void AdvancedGC::gc_doRead(uint64_t now, uint64_t tag) {
  LPN invlpn;
  auto &block = findCopySession(tag);

  if (LIKELY(block.pageReadIndex < block.copyList.size())) {
    // Do read
    auto &ctx = block.copyList.at(block.pageReadIndex++);
    auto ppn = param->makePPN(block.blockID, 0, ctx.pageIndex);

    if (superpage > 1) {
      debugprint(logid, "GC    | READ  | PSBN %" PRIx64 "h | PSPN %" PRIx64 "h",
                 block.blockID, param->getPSPNFromPPN(ppn));
    }
    else {
      debugprint(logid, "GC    | READ  | PBN %" PRIx64 "h | PPN %" PRIx64 "h",
                 block.blockID, ppn);
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
        pFIL->read(FIL::Request(invlpn, ppn,
                                makeBufferAddress(i, ctx.pageIndex),
                                eventDoTranslate, tag));
      }
    }

    block.readCounter = superpage;
    ctx.beginAt = now;
    stat.gcCopiedPages += superpage;

    increasePendingFIL();
  }
  else if (LIKELY(!preemptRequested())) {
    // Do erase
    PSBN psbn = block.blockID;

    // Erase
    if (superpage > 1) {
      debugprint(logid, "GC    | ERASE | PSBN %" PRIx64 "h", psbn);
    }
    else {
      // PSBN == PBN when superpage == 1
      debugprint(logid, "GC    | ERASE | PBN %" PRIx64 "h", psbn);
    }

    for (uint32_t i = 0; i < superpage; i++) {
      pFIL->erase(
          FIL::Request(invlpn, param->makePPN(psbn, i, 0), 0, eventDone, tag));
    }

    block.beginAt = now;
    block.writeCounter = superpage;  // Reuse
    stat.gcErasedBlocks += superpage;

    increasePendingFIL();
  }
  else {
    checkPreemptible();
  }
}

void AdvancedGC::gc_doTranslate(uint64_t now, uint64_t tag) {
  decreasePendingFIL();

  NaiveGC::gc_doTranslate(now, tag);
}

void AdvancedGC::gc_doWrite(uint64_t now, uint64_t tag) {
  NaiveGC::gc_doWrite(now, tag);

  increasePendingFIL();
}

void AdvancedGC::gc_writeDone(uint64_t now, uint64_t tag) {
  decreasePendingFIL();

  NaiveGC::gc_writeDone(now, tag);
}

void AdvancedGC::gc_done(uint64_t now, uint64_t tag) {
  decreasePendingFIL();

  NaiveGC::gc_done(now, tag);
}

void AdvancedGC::createCheckpoint(std::ostream &out) const noexcept {
  NaiveGC::createCheckpoint(out);

  BACKUP_EVENT(out, eventBackgroundGC);
}

void AdvancedGC::restoreCheckpoint(std::istream &in) noexcept {
  NaiveGC::restoreCheckpoint(in);

  RESTORE_EVENT(in, eventBackgroundGC);
}

}  // namespace SimpleSSD::FTL::GC
