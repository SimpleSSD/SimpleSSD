// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 *         Junhyeok Jang <jhjang@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_FIL_REQUEST_HH__
#define __SIMPLESSD_FIL_REQUEST_HH__

#include "ftl/def.hh"
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

  uint64_t memoryAddress;

  FTL::Request *parent;

 public:
  Request(LPN l, PPN p, uint64_t a, Event e, uint64_t d)
      : lpn(l), ppn(p), eid(e), data(d), memoryAddress(a), parent(nullptr) {}
  Request(FTL::Request *r, Event e)
      : lpn(r->getLPN()),
        ppn(r->getPPN()),
        eid(e),
        data(r->getTag()),
        memoryAddress(r->getDRAMAddress()),
        parent(r) {}

  inline uint64_t getTag() { return tag; }
  inline Operation getOpcode() { return opcode; }

  inline LPN getLPN() { return lpn; }
  inline PPN getPPN() { return ppn; }

  inline Event getEvent() { return eid; }
  inline uint64_t getEventData() { return data; }

  inline uint64_t getDRAMAddress() { return memoryAddress; }

  inline void setLPN(LPN l) {
    lpn = l;
    if (parent) {
      parent->setLPN(l);
    }
  }
};

using SuperRequest = std::vector<Request *>;

}  // namespace SimpleSSD::FIL

#endif
