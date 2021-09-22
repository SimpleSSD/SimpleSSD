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
               state == State::Idle)) {
    state = State::Background;
    beginAt = now;

    scheduleNow(eventTrigger);
  }
}

void AdvancedGC::requestArrived(Request *req) {
  // Penalty calculation
  NaiveGC::requestArrived(req);

  // Not scheduled
  if (LIKELY(state < State::Foreground)) {
    // Reschedule GC check
    if (isScheduled(eventBackgroundGC)) {
      deschedule(eventBackgroundGC);
    }

    scheduleRel(eventBackgroundGC, 0, idletime);
  }
}

void AdvancedGC::gc_trigger() {
  bool fgc = false;  // For message
  uint32_t size = 0;

  if (ftlobject.pAllocator->checkForegroundGCThreshold()) {
    stat.fgcCount++;
    state = State::Foreground;

    size = fgcBlocksToErase;
    fgc = true;
  }
  else if (state == State::Background) {
    stat.bgcCount++;
    state = State::Background;

    size = bgcBlocksToErase;
  }

  for (uint32_t idx = 0; idx < size; idx++) {
    ftlobject.pAllocator->getVictimBlocks(targetBlocks[idx], eventStart, idx);
  }

  if (fgc) {
    debugprint(logid, "GC    | Foreground | %u blocks", fgcBlocksToErase);
  }
  else {
    debugprint(logid, "GC    | Background | %u blocks", bgcBlocksToErase);
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
