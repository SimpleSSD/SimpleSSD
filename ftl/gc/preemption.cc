// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "ftl/gc/preemption.hh"

#include "ftl/allocator/abstract_allocator.hh"
#include "ftl/base/abstract_ftl.hh"

namespace SimpleSSD::FTL::GC {

PreemptibleGC::PreemptibleGC(ObjectData &o, FTLObjectData &fo, FIL::FIL *f)
    : AdvancedGC(o, fo, f),
      pendingFIL(0),
      preemptRequestedAt(std::numeric_limits<uint64_t>::max()) {
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

void PreemptibleGC::gc_checkDone(uint64_t now) {
  // Maybe GC is completed while waiting for pending requests
  checkPreemptible();

  AdvancedGC::gc_checkDone(now);
}

void PreemptibleGC::gc_doRead(uint64_t now, uint64_t tag) {
  if (LIKELY(!preemptRequested())) {
    AdvancedGC::gc_doRead(now, tag);

    increasePendingFIL();
  }
  else {
    checkPreemptible();
  }
}

void PreemptibleGC::gc_doTranslate(uint64_t now, uint64_t tag) {
  decreasePendingFIL();

  AdvancedGC::gc_doTranslate(now, tag);
}

void PreemptibleGC::gc_doWrite(uint64_t now, uint64_t tag) {
  AdvancedGC::gc_doWrite(now, tag);

  increasePendingFIL();
}

void PreemptibleGC::gc_writeDone(uint64_t now, uint64_t tag) {
  decreasePendingFIL();

  AdvancedGC::gc_writeDone(now, tag);
}

void PreemptibleGC::gc_done(uint64_t now, uint64_t tag) {
  decreasePendingFIL();

  AdvancedGC::gc_done(now, tag);
}

void PreemptibleGC::requestArrived(bool isread, uint32_t bytes) {
  // Penalty calculation & Background GC invocation
  AdvancedGC::requestArrived(isread, bytes);

  // Request preemption
  if (UNLIKELY(state >= State::Foreground && !preemptRequested())) {
    preemptRequestedAt = getTick();

    debugprint(logid, "GC    | Preemption requested");
  }
}

void PreemptibleGC::createCheckpoint(std::ostream &out) const noexcept {
  AdvancedGC::createCheckpoint(out);
}

void PreemptibleGC::restoreCheckpoint(std::istream &in) noexcept {
  AdvancedGC::restoreCheckpoint(in);
}

}  // namespace SimpleSSD::FTL::GC
