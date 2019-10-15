// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Jie Zhang <jie@camelab.org>
 *         Donghyun Gouk <kukdh1@camelab.org>
 */

#include "PAL2_TimeSlot.h"

TimeSlot::TimeSlot(uint64_t startTick, uint64_t duration) {
  StartTick = startTick;
  EndTick = startTick + duration - 1;
}
