// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_HIL_REQUEST_HH__
#define __SIMPLESSD_HIL_REQUEST_HH__

#include "hil/common/dma_engine.hh"

// Use SimpleSSD namespace instead of SimpleSSD::HIL
namespace SimpleSSD {

// Forward definition
namespace HIL {

class HIL;

}

enum class Operation : uint16_t {
  None,             // NVMCplt. DMACplt.
  Read,             // NLP      NLP
  Write,            // NLP      NLP
  WriteZeroes,      // NLP      0
  Compare,          // NLP      NLP
  CompareAndWrite,  // NLP      NLP
  Flush,            // 1        0
  Trim,             // 1        0
  Format,           // 1        0
};

class Request {
 private:
  friend HIL::HIL;

  Operation opcode;

  HIL::DMAEngine *dmaEngine;
  HIL::DMATag dmaTag;

  Event eid;
  uint64_t data;

  uint64_t offset;  //!< Byte offset
  uint32_t length;  //!< Byte length

  uint32_t dmaCounter;
  uint32_t nvmCounter;
  uint32_t nlp;  //!< # logical pages

  uint64_t dmaBeginAt;
  uint64_t nvmBeginAt;

  uint64_t requestTag;

 public:
  Request(Event e, uint64_t c)
      : opcode(Operation::None),
        dmaEngine(nullptr),
        dmaTag(HIL::InvalidDMATag),
        eid(e),
        data(c),
        offset(0),
        length(0),
        nlp(0),
        dmaCounter(0),
        nvmCounter(0),
        dmaBeginAt(0),
        nvmBeginAt(0),
        requestTag(0) {}
  Request(HIL::DMAEngine *d, HIL::DMATag t, Event e, uint64_t c)
      : opcode(Operation::None),
        dmaEngine(d),
        dmaTag(t),
        eid(e),
        data(c),
        offset(0),
        length(0),
        nlp(0),
        dmaCounter(0),
        nvmCounter(0),
        dmaBeginAt(0),
        nvmBeginAt(0),
        requestTag(0) {}

  void setAddress(LPN slpn, uint32_t nlp, uint32_t lbaSize) {
    offset = slpn * lbaSize;
    length = nlp * lbaSize;
  }

  void setAddress(uint64_t byteoffset, uint32_t bytelength) {
    offset = byteoffset;
    length = bytelength;
  }
};

class SubRequest {
 private:
  friend HIL::HIL;

  const uint64_t requestTag;

  Request *request;

  LPN lpn;

  uint64_t offset;
  uint32_t length;

 public:
  SubRequest(uint64_t t, Request *r) : requestTag(t), request(r) {}
  SubRequest(uint64_t t, Request *r, LPN l, uint64_t o, uint32_t s)
      : requestTag(t), request(r), lpn(l), offset(o), length(s) {}

  inline const uint64_t getTag() { return requestTag; }
};

}  // namespace SimpleSSD

#endif