// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Jie Zhang <jie@camelab.org>
 *         Donghyun Gouk <kukdh1@camelab.org>
 */

#include "PAL2_TimeSlot.h"

#include "sim/checkpoint.hh"

using namespace SimpleSSD;

TimeSlot::TimeSlot(uint64_t startTick, uint64_t duration) {
  StartTick = startTick;
  EndTick = startTick + duration - 1;
}

void TimeSlot::backup(std::ostream &out) const {
  BACKUP_SCALAR(out, StartTick);
  BACKUP_SCALAR(out, EndTick);
}

void TimeSlot::restore(std::istream &in) {
  RESTORE_SCALAR(in, StartTick);
  RESTORE_SCALAR(in, EndTick);
}
