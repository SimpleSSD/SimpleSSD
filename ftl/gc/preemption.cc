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
        scheduleNow(eventReadPage, idx);
      }
      else {
        panic("Unexpected GC preemption state");
      }
    }
  }
}

void PreemptibleGC::done(uint64_t now, uint32_t idx) {
  // Maybe GC is completed while waiting for pending requests
  if (UNLIKELY(preemptRequested())) {
    checkPreemptible();
  }

  AdvancedGC::done(now, idx);
}

void PreemptibleGC::readPage(uint64_t now, uint32_t idx) {
  if (LIKELY(!preemptRequested() || state == State::Foreground)) {
    AdvancedGC::readPage(now, idx);

    increasePendingFIL(idx);
  }
  else {
    checkPreemptible();
  }
}

void PreemptibleGC::updateMapping(uint64_t now, uint32_t idx) {
  decreasePendingFIL(idx);

  AdvancedGC::updateMapping(now, idx);
}

void PreemptibleGC::writePage(uint64_t now, uint32_t idx) {
  AdvancedGC::writePage(now, idx);

  increasePendingFIL(idx);
}

void PreemptibleGC::writeDone(uint64_t now, uint32_t idx) {
  decreasePendingFIL(idx);

  AdvancedGC::writeDone(now, idx);
}

void PreemptibleGC::eraseDone(uint64_t now, uint32_t idx) {
  decreasePendingFIL(idx);

  AdvancedGC::eraseDone(now, idx);
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

  BACKUP_STL(out, pendingFILs, iter, BACKUP_SCALAR(out, iter));
}

void PreemptibleGC::restoreCheckpoint(std::istream &in) noexcept {
  AdvancedGC::restoreCheckpoint(in);

  RESTORE_STL_RESIZE(in, pendingFILs, i, RESTORE_SCALAR(in, pendingFILs[i]));
}

}  // namespace SimpleSSD::FTL::GC
