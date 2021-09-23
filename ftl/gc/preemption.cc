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
    : AdvancedGC(o, fo, f) {
  logid = Log::DebugID::FTL_PreemptibleGC;

  pendingFILs.resize(targetBlocks.size());
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
  bool msg = false;

  const uint32_t size = targetBlocks.size();

  for (uint32_t idx = 0; idx < size; idx++) {
    auto &targetBlock = targetBlocks[idx];

    if (LIKELY(targetBlock.blockID.isValid())) {
      if (!msg) {
        debugprint(logid, "GC    | Resume from preempted state");
        msg = true;
      }

      panic_if(targetBlock.writeCounter != 0 || targetBlock.readCounter != 0,
               "Unexpected GC preemption state");

      if (targetBlock.pageWriteIndex == targetBlock.pageReadIndex) {
        scheduleNow(eventDoRead, idx);
      }
      else {
        panic("Unexpected GC preemption state");
      }
    }
  }
}

void PreemptibleGC::gc_done(uint64_t now, uint32_t idx) {
  // Maybe GC is completed while waiting for pending requests
  if (UNLIKELY(preemptRequested())) {
    checkPreemptible();
  }

  AdvancedGC::gc_done(now, idx);
}

void PreemptibleGC::gc_doRead(uint64_t now, uint32_t idx) {
  if (LIKELY(!preemptRequested() || state == State::Foreground)) {
    AdvancedGC::gc_doRead(now, idx);

    increasePendingFIL(idx);
  }
  else {
    checkPreemptible();
  }
}

void PreemptibleGC::gc_doTranslate(uint64_t now, uint32_t idx) {
  decreasePendingFIL(idx);

  AdvancedGC::gc_doTranslate(now, idx);
}

void PreemptibleGC::gc_doWrite(uint64_t now, uint32_t idx) {
  AdvancedGC::gc_doWrite(now, idx);

  increasePendingFIL(idx);
}

void PreemptibleGC::gc_writeDone(uint64_t now, uint32_t idx) {
  decreasePendingFIL(idx);

  AdvancedGC::gc_writeDone(now, idx);
}

void PreemptibleGC::gc_eraseDone(uint64_t now, uint32_t idx) {
  decreasePendingFIL(idx);

  AdvancedGC::gc_eraseDone(now, idx);
}

void PreemptibleGC::requestArrived(Request *req) {
  // Request preemption
  if (UNLIKELY(state >= State::Background && !preemptRequested())) {
    debugprint(logid, "GC    | Preemption requested");
  }

  // Penalty calculation & Background GC invocation
  AdvancedGC::requestArrived(req);
}

void PreemptibleGC::createCheckpoint(std::ostream &out) const noexcept {
  AdvancedGC::createCheckpoint(out);

  for (const auto &iter : pendingFILs) {
    BACKUP_SCALAR(out, iter);
  }
}

void PreemptibleGC::restoreCheckpoint(std::istream &in) noexcept {
  AdvancedGC::restoreCheckpoint(in);

  uint64_t size = targetBlocks.size();

  pendingFILs.resize(size);

  for (uint64_t i = 0; i < size; i++) {
    RESTORE_SCALAR(in, pendingFILs[i]);
  }
}

}  // namespace SimpleSSD::FTL::GC
