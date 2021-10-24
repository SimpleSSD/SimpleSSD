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
  if (UNLIKELY(state >= State::Foreground)) {
    // GC in progress
    firstRequestArrival = std::numeric_limits<uint64_t>::max();
  }
  else {
    triggerBackground(now);
  }
}

void AdvancedGC::requestArrived(Request *req) {
  // Penalty calculation
  NaiveGC::requestArrived(req);
}

void AdvancedGC::trigger() {
  bool fgc = false;  // For message
  uint32_t size = 0;

  if (ftlobject.pAllocator->checkForegroundGCThreshold()) {
    stat.fgcCount++;
    state = State::Foreground;

    size = fgcBlocksToErase;
    fgc = true;
  }
  else {
    stat.bgcCount++;
    state = State::Background;

    size = bgcBlocksToErase;
  }

  for (uint32_t idx = 0; idx < size; idx++) {
    ftlobject.pAllocator->getVictimBlock(targetBlocks[idx], method,
                                         eventReadPage, idx);
  }

  if (fgc) {
    debugprint(logid, "GC    | Foreground | %u blocks", fgcBlocksToErase);
  }
  else {
    debugprint(logid, "GC    | Background | %u blocks", bgcBlocksToErase);
  }
}

void AdvancedGC::done(uint64_t now, uint32_t idx) {
  bool conflicted = firstRequestArrival != std::numeric_limits<uint64_t>::max();

  NaiveGC::done(now, idx);

  if (state == State::Idle) {
    if (!conflicted) {
      // As no request is submitted while GC, continue for background GC
      triggerBackground(now);
    }
  }
}

}  // namespace SimpleSSD::FTL::GC
