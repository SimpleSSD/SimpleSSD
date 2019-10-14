// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_FIL_DEF_HH__
#define __SIMPLESSD_FIL_DEF_HH__

#include <cinttypes>
#include <functional>

namespace SimpleSSD::FIL {

enum PageAllocation : uint8_t {
  None = 0,
  Channel = 1,
  Way = 2,
  Die = 4,
  Plane = 8,
  All = 15,
};

enum Index : uint8_t {
  Level1,
  Level2,
  Level3,
  Level4,
};

}  // namespace SimpleSSD::FIL

#endif
