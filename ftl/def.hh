// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_FTL_DEF_HH__
#define __SIMPLESSD_FTL_DEF_HH__

#include <cinttypes>

#include "sim/event.hh"

namespace SimpleSSD::FTL {

typedef struct {
  uint64_t totalPhysicalPages;
  uint64_t totalLogicalPages;
  uint32_t pageSize;
  uint32_t parallelismLevel[4];  //!< Parallelism group list
  uint8_t superpageLevel;  //!< Number of levels (1~N) included in superpage
} Parameter;

enum class Operation : uint8_t {
  Read,
  Write,
  Trim,
  Format,
};

struct Request {
  uint64_t id;

  Event eid;
  uint64_t data;

  Operation opcode;

  uint64_t address;
  union {
    uint8_t *buffer;
    uint64_t length;  // Used for Trim/Format
  };

  Request();
  Request(uint64_t, Event, uint64_t, Operation, uint64_t, uint8_t *);
  Request(uint64_t, Event, uint64_t, Operation, uint64_t, uint64_t);
};

}  // namespace SimpleSSD::FTL

#endif
