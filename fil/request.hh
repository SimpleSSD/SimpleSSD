// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_FIL_REQUEST_HH__
#define __SIMPLESSD_FIL_REQUEST_HH__

#include "sim/object.hh"

namespace SimpleSSD::FIL {

enum class Operation : uint8_t {
  None,
  Read,
  ReadCache,
  ReadCopyback,
  Program,
  ProgramCache,
  ProgramCopyback,
  Erase,
};

class Request {
 private:
  friend FIL;

  uint64_t requestTag;

  bool multiplane;
  Operation opcode;

  PPN address;

  uint8_t *buffer;

  Event eid;
  uint64_t data;

 public:
  Request(PPN a, Event e, uint64_t d)
      : multiplane(false),
        opcode(Operation::None),
        address(a),
        buffer(nullptr),
        eid(e),
        data(d) {}

  inline const uint64_t getTag() { return requestTag; }
  inline bool getMultiPlane() { return multiplane; }
  inline Operation getOperation() { return opcode; }
  inline PPN getAddress() { return address; }
  inline const uint8_t *getData() { return buffer; }

  inline void setData(const uint8_t *ptr) { buffer = (uint8_t *)ptr; }
};

}  // namespace SimpleSSD::FIL

#endif
