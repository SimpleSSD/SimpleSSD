// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "ftl/gc/preemption.hh"

#include "ftl/allocator/abstract_allocator.hh"

namespace SimpleSSD::FTL::GC {

PreemptibleGC::PreemptibleGC(ObjectData &o, FTLObjectData &fo, FIL::FIL *f)
    : AdvancedGC(o, fo, f), pendingFIL(0) {
  logid = Log::DebugID::FTL_PreemptibleGC;
}

PreemptibleGC::~PreemptibleGC() {}

void PreemptibleGC::triggerBackground(uint64_t now) {
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

void PreemptibleGC::triggerForeground() {
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

void PreemptibleGC::resumePaused() {
  if (LIKELY(targetBlock.blockID.isValid())) {
    debugprint(logid, "GC    | Resume from preempted state");

    panic_if(targetBlock.writeCounter != 0 || targetBlock.readCounter != 0,
             "Unexpected GC preemption state");

    if (targetBlock.pageWriteIndex == targetBlock.pageReadIndex) {
      scheduleNow(eventDoRead);
    }
    else {
      panic("Unexpected GC preemption state");
    }
  }
}

void PreemptibleGC::gc_checkDone(uint64_t now) {
  // Maybe GC is completed while waiting for pending requests
  checkPreemptible();

  AdvancedGC::gc_checkDone(now);
}

void PreemptibleGC::gc_doRead(uint64_t now) {
  if (LIKELY(!preemptRequested())) {
    // Don't use AdvancedGC::gc_doRead
    NaiveGC::gc_doRead(now);

    increasePendingFIL();
  }
  else {
    checkPreemptible();
  }
}

void PreemptibleGC::gc_doTranslate(uint64_t now) {
  decreasePendingFIL();

  AdvancedGC::gc_doTranslate(now);
}

void PreemptibleGC::gc_doWrite(uint64_t now) {
  AdvancedGC::gc_doWrite(now);

  increasePendingFIL();
}

void PreemptibleGC::gc_writeDone(uint64_t now) {
  decreasePendingFIL();

  AdvancedGC::gc_writeDone(now);
}

void PreemptibleGC::gc_eraseDone(uint64_t now) {
  decreasePendingFIL();

  AdvancedGC::gc_done(now);
}

void PreemptibleGC::requestArrived(Request *req) {
  // Request preemption
  if (UNLIKELY(state >= State::Foreground && !preemptRequested())) {
    debugprint(logid, "GC    | Preemption requested");
  }

  // Penalty calculation & Background GC invocation
  AdvancedGC::requestArrived(req);
}

void PreemptibleGC::createCheckpoint(std::ostream &out) const noexcept {
  AdvancedGC::createCheckpoint(out);

  BACKUP_SCALAR(out, pendingFIL);
}

void PreemptibleGC::restoreCheckpoint(std::istream &in) noexcept {
  AdvancedGC::restoreCheckpoint(in);

  RESTORE_SCALAR(in, pendingFIL);
}

}  // namespace SimpleSSD::FTL::GC
