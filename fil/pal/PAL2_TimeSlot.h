// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Jie Zhang <jie@camelab.org>
 *         Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __PAL2_TimeSlot_h__
#define __PAL2_TimeSlot_h__

#include "SimpleSSD_types.h"

#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <fstream>
#include <iostream>
#include <string>
using namespace std;

struct TimeSlot {
  uint64_t StartTick;
  uint64_t EndTick;

  TimeSlot(uint64_t startTick, uint64_t duration);
  TimeSlot() : StartTick(0ull), EndTick(0ull){};
};

#endif
