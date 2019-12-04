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

enum class Response : uint16_t {
  Success,
  Unwritten,         //!< Specified LBA range is not written (All commands)
  OutOfRange,        //!< Specified LBA range is out-of-range (All commands)
  FormatInProgress,  //!< Format in progress (All commands)
  ReadECCFail,       //!< Read ECC failed (Read commands only)
  WriteFail,         //!< Write failed (Write commands only)
  CompareFail,       //!< Compare failed (Compare / Fused write only)
};

class Request {
 private:
  friend HIL::HIL;

  Operation opcode;
  Response result;  //!< Request result

  uint32_t lbaSize;  //!< Not used by HIL

  HIL::DMAEngine *dmaEngine;
  HIL::DMATag dmaTag;

  Event eid;      //!< Completion event
  uint64_t data;  //!< Completion data

  uint64_t offset;  //!< Byte offset
  uint32_t length;  //!< Byte length

  uint32_t dmaCounter;  //!< # completed DMA request
  uint32_t nvmCounter;  //!< # completed NVM (ICL) request
  uint32_t nlp;         //!< # logical pages

  uint64_t dmaBeginAt;
  uint64_t nvmBeginAt;

  uint64_t requestTag;  //!< Unique ID for HIL

 public:
  Request()
      : opcode(Operation::None),
        result(Response::Success),
        lbaSize(0),
        dmaEngine(nullptr),
        dmaTag(HIL::InvalidDMATag),
        eid(InvalidEventID),
        data(0),
        offset(0),
        length(0),
        dmaCounter(0),
        nvmCounter(0),
        nlp(0),
        dmaBeginAt(0),
        nvmBeginAt(0),
        requestTag(0) {}
  Request(Event e, uint64_t c)
      : opcode(Operation::None),
        result(Response::Success),
        lbaSize(0),
        dmaEngine(nullptr),
        dmaTag(HIL::InvalidDMATag),
        eid(e),
        data(c),
        offset(0),
        length(0),
        dmaCounter(0),
        nvmCounter(0),
        nlp(0),
        dmaBeginAt(0),
        nvmBeginAt(0),
        requestTag(0) {}

  inline void setAddress(uint64_t slba, uint32_t nlb, uint32_t lbs) {
    lbaSize = lbs;
    offset = slba * lbaSize;
    length = nlb * lbaSize;
  }

  inline void setDMA(HIL::DMAEngine *engine, HIL::DMATag tag) {
    dmaEngine = engine;
    dmaTag = tag;
  }

  inline void getAddress(uint64_t &slba, uint32_t &nlb) {
    slba = offset / lbaSize;
    nlb = length / lbaSize;
  }

  inline HIL::DMATag getDMA() { return dmaTag; }
  inline Response getResponse() { return result; }
};

class SubRequest {
 private:
  friend HIL::HIL;

  const uint64_t requestTag;

  Request *request;

  LPN lpn;

  uint64_t offset;  //!< Offset in DMA Tag
  uint32_t length;  //!< Length in DMA Tag

  bool clear;
  uint8_t *buffer;  //!< Buffer for DMA

 public:
  SubRequest(uint64_t t, Request *r)
      : requestTag(t), request(r), clear(false), buffer(nullptr) {}
  SubRequest(uint64_t t, Request *r, LPN l, uint64_t o, uint32_t s)
      : requestTag(t),
        request(r),
        lpn(l),
        offset(o),
        length(s),
        clear(false),
        buffer(nullptr) {}
  SubRequest(const SubRequest &) = delete;
  SubRequest(SubRequest &&rhs) noexcept
      : requestTag(rhs.requestTag), clear(false), buffer(nullptr) {
    *this = std::move(rhs);
  }
  ~SubRequest() {
    if (clear) {
      free(buffer);
    }
  }

  SubRequest &operator=(const SubRequest &) = delete;
  SubRequest &operator=(SubRequest &&rhs) {
    if (this != &rhs) {
      this->request = std::exchange(rhs.request, nullptr);
      this->lpn = std::exchange(rhs.lpn, 0);
      this->offset = std::exchange(rhs.offset, 0);
      this->length = std::exchange(rhs.length, 0);
      this->clear = std::exchange(rhs.clear, false);
      this->buffer = std::exchange(rhs.buffer, nullptr);
    }

    return *this;
  }

  void setBuffer(uint8_t *data) { buffer = data; }
  void createBuffer(uint32_t size) {
    clear = true;

    buffer = (uint8_t *)calloc(size, 1);
  }

  inline uint64_t getTag() { return requestTag; }
  inline LPN getLPN() { return lpn; }
  inline const uint8_t *getBuffer() { return buffer; }
};

}  // namespace SimpleSSD

#endif
