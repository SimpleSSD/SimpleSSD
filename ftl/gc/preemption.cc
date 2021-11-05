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
    : AdvancedGC(o, fo, f), lastState(State::Idle) {}

PreemptibleGC::~PreemptibleGC() {}

void PreemptibleGC::initialize(bool restore) {
  configure(Log::DebugID::FTL_PreemptibleGC, "GC    ", "FTL::GC",
            getParallelBlockCount());

  pendingFILs.resize(targetBlocks.size());

  AdvancedGC::initialize(restore);
}

void PreemptibleGC::triggerBackground(uint64_t now) {
  if (UNLIKELY(ftlobject.pAllocator->checkBackgroundGCThreshold() &&
               state < State::Foreground)) {
    if (state == State::Paused) {
      state = lastState;

      resumePaused();
    }
    else {
      state = State::Background;

      scheduleNow(eventTrigger);
    }

    beginAt = now;
  }
}

void PreemptibleGC::triggerForeground() {
  if (UNLIKELY(ftlobject.pAllocator->checkForegroundGCThreshold() &&
               state < State::Foreground)) {
    if (state == State::Paused) {
      state = lastState;

      resumePaused();

      if (lastState == State::Background &&
          fgcBlocksToErase > bgcBlocksToErase) {
        // Start more blocks
        for (uint32_t idx = bgcBlocksToErase; idx < fgcBlocksToErase; idx++) {
          ftlobject.pAllocator->getVictimBlock(targetBlocks[idx], method,
                                               eventReadPage, idx);
        }

        // Mark as foreground
        state = State::Foreground;

        debugprint(logid, "GC    | Foreground | +%u blocks",
                   fgcBlocksToErase - bgcBlocksToErase);
      }
    }
    else {
      state = State::Foreground;

      scheduleNow(eventTrigger);
    }

    beginAt = getTick();
  }
}

void PreemptibleGC::resumePaused() {
  bool msg = false;
  uint32_t size;

  if (state == State::Foreground) {
    size = fgcBlocksToErase;
  }
  else {
    size = bgcBlocksToErase;
  }

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

  panic_if(!msg, "Resumed from preempted state, but none of page is read.");
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

void PreemptibleGC::done(uint64_t now, uint32_t idx) {
  bool allinvalid = true;
  auto &targetBlock = targetBlocks[idx];
  bool conflicted = preemptRequested();

  targetBlock.blockID.invalidate();

  // Check all GC has been completed
  for (auto &iter : targetBlocks) {
    if (iter.blockID.isValid()) {
      allinvalid = false;
      break;
    }
  }

  if (allinvalid) {
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

    // Calculate penalty
    updatePenalty(now);

    // If preemption not requested
    if (!conflicted) {
      // As we got new freeblock, restart `some of` stalled requests
      // This will trigger foreground GC again if necessary
      ftlobject.pFTL->restartStalledRequests();

      // If foreground GC was not invoked,
      if (state == State::Idle) {
        // and we are still in idle,
        if (!conflicted) {
          // continue for background GC
          triggerBackground(now);
        }
      }
    }
    else {
      // Preemption was requested but GC is completed
      debugprint(logid, "GC    | Preempted");

      firstRequestArrival = std::numeric_limits<uint64_t>::max();
    }
  }
  else if (UNLIKELY(conflicted)) {
    checkPreemptible();
  }
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
  BACKUP_SCALAR(out, lastState);
}

void PreemptibleGC::restoreCheckpoint(std::istream &in) noexcept {
  AdvancedGC::restoreCheckpoint(in);

  RESTORE_STL_RESIZE(in, pendingFILs, i, RESTORE_SCALAR(in, pendingFILs[i]));
  RESTORE_SCALAR(in, lastState);
}

}  // namespace SimpleSSD::FTL::GC
