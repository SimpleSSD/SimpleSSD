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

void AdvancedGC::gc_checkDone(uint64_t now) {
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
  triggerBackground(now);
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

  BACKUP_EVENT(out, eventBackgroundGC);
}

void AdvancedGC::restoreCheckpoint(std::istream &in) noexcept {
  NaiveGC::restoreCheckpoint(in);

  RESTORE_EVENT(in, eventBackgroundGC);
}

}  // namespace SimpleSSD::FTL::GC
