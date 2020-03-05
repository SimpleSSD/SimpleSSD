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

  uint64_t tag;
  Operation opcode;

  LPN lpn;
  PPN ppn;

  Event eid;
  uint64_t data;

 public:
  Request(PPN p, Event e, uint64_t d)
      : lpn(InvalidLPN), ppn(p), eid(e), data(d) {}

  inline uint64_t getTag() { return tag; }

  inline LPN getLPN() { return lpn; }
  inline PPN getPPN() { return ppn; }

  inline Event getEvent() { return eid; }
  inline uint64_t getEventData() { return data; }

  inline void setLPN(LPN l) { lpn = l; }
};

}  // namespace SimpleSSD::FIL

#endif
