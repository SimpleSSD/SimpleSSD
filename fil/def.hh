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

#include "sim/event.hh"

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

enum class Operation : uint8_t {
  Read,
  Program,
  Erase,
  Copyback,
};

struct Request {
  uint64_t id;

  Event eid;
  uint64_t data;

  Operation opcode;

  uint64_t address;
  uint8_t *buffer;

  std::vector<uint8_t> spare;

  Request();
  Request(uint64_t, Event, uint64_t, Operation, uint64_t, uint8_t *);
  Request(uint64_t, Event, uint64_t, Operation, uint64_t, uint8_t *,
          std::vector<uint8_t> &);
};

}  // namespace SimpleSSD::FIL

#endif
