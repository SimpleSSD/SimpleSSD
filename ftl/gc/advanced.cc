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
    : NaiveGC(o, fo, f) {
  logid = Log::DebugID::FTL_AdvancedGC;

  idletime =
      readConfigUint(Section::FlashTranslation, Config::Key::BGCIdleTime);

  // Create event
  eventBackgroundGC =
      createEvent([this](uint64_t t, uint64_t) { triggerBackground(t); },
                  "FTL::GC::eventBackgroundGC");

  // Schedule
  scheduleRel(eventBackgroundGC, 0, idletime);
}

AdvancedGC::~AdvancedGC() {}

void AdvancedGC::triggerBackground(uint64_t now) {
  if (ftlobject.pAllocator->checkBackgroundGCThreshold() &&
      state == State::Idle) {
    state = State::Background;
    beginAt = now;

    scheduleNow(eventTrigger);
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

void AdvancedGC::gc_start(uint64_t now) {
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
    if (firstRequestArrival < now) {
      auto penalty = now - firstRequestArrival;

      stat.penalty_count++;
      stat.avg_penalty += penalty;
      stat.min_penalty = MIN(stat.min_penalty, penalty);
      stat.max_penalty = MAX(stat.max_penalty, penalty);

      firstRequestArrival = std::numeric_limits<uint64_t>::max();
    }

    // Check threshold
    triggerBackground(now);

    if (state == State::Idle) {
      // Not triggered
      ftlobject.pFTL->restartStalledRequests();
    }
  }
}

void AdvancedGC::requestArrived(bool isread, uint32_t bytes) {
  // Penalty calculation
  NaiveGC::requestArrived(isread, bytes);

  // Not scheduled and GC is not in progress
  if (!isScheduled(eventBackgroundGC) &&
      beginAt == std::numeric_limits<uint64_t>::max()) {
    scheduleRel(eventBackgroundGC, 0, idletime);
  }
}

void AdvancedGC::createCheckpoint(std::ostream &out) const noexcept {
  NaiveGC::createCheckpoint(out);
}

void AdvancedGC::restoreCheckpoint(std::istream &in) noexcept {
  NaiveGC::restoreCheckpoint(in);
}

}  // namespace SimpleSSD::FTL::GC
