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

struct Parameter {
  uint32_t channel;          //!< Total # channels
  uint32_t package;          //!< # packages / channel
  uint32_t die;              //!< # dies / package
  uint32_t plane;            //!< # planes / die
  uint32_t block;            //!< # blocks / plane
  uint32_t page;             //!< # pages / block
  uint32_t superBlock;       //!< Total super blocks
  uint32_t pageSize;         //!< Size of page in bytes
  uint32_t superPageSize;    //!< Size of super page in bytes
  uint32_t pageInSuperPage;  //!< # pages in one superpage
};

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
