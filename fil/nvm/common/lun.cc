// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "fil/nvm/common/lun.hh"

namespace SimpleSSD::FIL::NVM {

LUN::LUN(ObjectData &o)
    : Object(o),
      state(State::Invalid),
      nextState(State::Invalid),
      transitAt(0) {}

LUN::~LUN() {}

State LUN::getCurrentState() noexcept {
  return state;
}

State LUN::getNextState() noexcept {
  return nextState;
}

void LUN::createCheckpoint(std::ostream &out) const noexcept {
  BACKUP_SCALAR(out, state);
  BACKUP_SCALAR(out, nextState);
  BACKUP_SCALAR(out, transitAt);
}

void LUN::restoreCheckpoint(std::istream &in) noexcept {
  RESTORE_SCALAR(in, state);
  RESTORE_SCALAR(in, nextState);
  RESTORE_SCALAR(in, transitAt);
}

}  // namespace SimpleSSD::FIL::NVM
