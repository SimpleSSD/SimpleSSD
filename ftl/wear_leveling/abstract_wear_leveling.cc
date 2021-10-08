// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "ftl/wear_leveling/abstract_wear_leveling.hh"

namespace SimpleSSD::FTL::WearLeveling {

AbstractWearLeveling::AbstractWearLeveling(ObjectData &o, FTLObjectData &fo,
                                           FIL::FIL *fil)
    : AbstractBlockCopyJob(o, fo, fil), state(State::Idle) {
  eventEraseCallback = createEvent(
      [this](uint64_t t, uint64_t d) { blockEraseCallback(t, PSBN{d}); },
      "FTL::WearLeveling::eventEraseCallback");
}

AbstractWearLeveling::~AbstractWearLeveling() {}

void AbstractWearLeveling::blockEraseCallback(uint64_t, const PSBN &) {
  panic("AbstractWearLeveling::blockEraseCallback() must be overriden.");
}

void AbstractWearLeveling::initialize() {}

bool AbstractWearLeveling::isRunning() {
  return state >= State::Foreground;
}

void AbstractWearLeveling::createCheckpoint(std::ostream &out) const noexcept {
  AbstractBlockCopyJob::createCheckpoint(out);

  BACKUP_SCALAR(out, state);
  BACKUP_EVENT(out, eventEraseCallback);
}

void AbstractWearLeveling::restoreCheckpoint(std::istream &in) noexcept {
  AbstractBlockCopyJob::restoreCheckpoint(in);

  RESTORE_SCALAR(in, state);
  RESTORE_EVENT(in, eventEraseCallback);
}

}  // namespace SimpleSSD::FTL::WearLeveling
