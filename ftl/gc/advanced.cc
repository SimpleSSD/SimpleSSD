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
    : NaiveGC(o, fo, f) {}

AdvancedGC::~AdvancedGC() {}

void AdvancedGC::initialize(bool restore) {
  configure(Log::DebugID::FTL_AdvancedGC, "GC    ", "FTL::GC",
            getParallelBlockCount());

  NaiveGC::initialize(restore);
}

void AdvancedGC::triggerBackground(uint64_t now) {
  if (UNLIKELY(ftlobject.pAllocator->checkBackgroundGCThreshold() &&
               state == State::Idle)) {
    state = State::Background;
    beginAt = now;

    scheduleNow(eventTrigger);
  }
}

void AdvancedGC::triggerByIdle(uint64_t now, uint64_t) {
  if (UNLIKELY(state < State::Foreground)) {
    triggerBackground(now);
  }
}

void AdvancedGC::requestArrived(Request *req) {
  // Penalty calculation
  NaiveGC::requestArrived(req);
}

void AdvancedGC::trigger() {
  uint32_t size = 0;

  if (state == State::Foreground) {
    stat.fgcCount++;

    size = fgcBlocksToErase;
  }
  else {
    stat.bgcCount++;

    size = bgcBlocksToErase;
  }

  for (uint32_t idx = 0; idx < size; idx++) {
    ftlobject.pAllocator->getVictimBlock(targetBlocks[idx], method,
                                         eventReadPage, idx);
  }

  if (state == State::Foreground) {
    debugprint(logid, "GC    | Foreground | %u blocks", fgcBlocksToErase);
  }
  else {
    debugprint(logid, "GC    | Background | %u blocks", bgcBlocksToErase);
  }
}

void AdvancedGC::done(uint64_t now, uint32_t idx) {
  bool allinvalid = true;
  auto &targetBlock = targetBlocks[idx];

  // True if we got request while in GC
  bool conflicted = firstRequestArrival != std::numeric_limits<uint64_t>::max();

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

    // As we got new freeblock, restart `some of` stalled requests
    // This will trigger foreground GC again if necessary
    auto restarted = ftlobject.pFTL->restartStalledRequests();

    // If foreground GC was not invoked,
    if (state == State::Idle) {
      // and we are still in idle,
      if (!conflicted && !restarted) {
        // continue for background GC
        triggerBackground(now);
      }
    }
  }
}

}  // namespace SimpleSSD::FTL::GC
