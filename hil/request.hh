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

namespace SimpleSSD::HIL {

// Forward definition
class HIL;
class SubRequest;

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

const char *getOperationName(Operation);

class Request {
 private:
  friend HIL;
  friend SubRequest;

  Operation opcode;
  Response result;  //!< Request result

  bool clear;  //!< Flag for buffer management

  uint32_t lbaSize;  //!< Not used by HIL

  DMAEngine *dmaEngine;
  DMATag dmaTag;

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
  uint64_t hostTag;     //!< Host tag info <u32:ctrl><u16:queue><u16:entry>

  LPN slpn;  //!< Starting logical page number

  uint64_t firstSubRequestTag;

  uint8_t *buffer;  //!< Buffer for DMA (real data)

 public:
  Request();
  Request(Event, uint64_t);
  Request(const Request &) = delete;
  Request(Request &&rhs) noexcept : clear(false), buffer(nullptr) {
    *this = std::move(rhs);
  }
  ~Request() {
    if (clear) {
      free(buffer);
    }
  }

  Request &operator=(const Request &) = delete;
  Request &operator=(Request &&rhs) {
    if (this != &rhs) {
      this->opcode = std::exchange(rhs.opcode, Operation::None);
      this->result = std::exchange(rhs.result, Response::Success);
      this->clear = std::exchange(rhs.clear, 0);
      this->lbaSize = std::exchange(rhs.lbaSize, 0);
      this->dmaEngine = std::exchange(rhs.dmaEngine, nullptr);
      this->dmaTag = std::exchange(rhs.dmaTag, nullptr);
      this->eid = std::exchange(rhs.eid, nullptr);
      this->data = std::exchange(rhs.data, 0);
      this->offset = std::exchange(rhs.offset, 0);
      this->length = std::exchange(rhs.length, 0);
      this->dmaCounter = std::exchange(rhs.dmaCounter, 0);
      this->nvmCounter = std::exchange(rhs.nvmCounter, 0);
      this->nlp = std::exchange(rhs.nlp, 0);
      this->dmaBeginAt = std::exchange(rhs.dmaBeginAt, 0);
      this->nvmBeginAt = std::exchange(rhs.nvmBeginAt, 0);
      this->requestTag = std::exchange(rhs.requestTag, 0);
      this->hostTag = std::exchange(rhs.hostTag, 0);
      this->slpn = std::exchange(rhs.slpn, 0);
      this->firstSubRequestTag = std::exchange(rhs.firstSubRequestTag, 0);
      this->buffer = std::exchange(rhs.buffer, nullptr);
    }

    return *this;
  }

  inline void setHostTag(uint64_t tag) { hostTag = tag; }

  inline void setAddress(uint64_t slba, uint32_t nlb, uint32_t lbs) {
    lbaSize = lbs;
    offset = slba * lbaSize;
    length = nlb * lbaSize;
  }

  inline void setDMA(DMAEngine *engine, DMATag tag) {
    dmaEngine = engine;
    dmaTag = tag;
  }

  void setBuffer(uint8_t *data) { buffer = data; }
  void createBuffer() {
    if (!clear) {
      clear = true;

      buffer = (uint8_t *)calloc(length, 1);
    }
  }

  inline uint8_t *getBuffer() { return buffer; }

  inline void getAddress(uint64_t &slba, uint32_t &nlb) {
    slba = offset / lbaSize;
    nlb = length / lbaSize;
  }

  inline DMATag getDMA() { return dmaTag; }
  inline Response getResponse() { return result; }

  inline DMATag getDMA() const { return dmaTag; }
  inline uint64_t getTag() const { return requestTag; }

  void createCheckpoint(std::ostream &out) const noexcept;
  void restoreCheckpoint(std::istream &in, ObjectData &object) noexcept;
};

class SubRequest {
 private:
  friend HIL;

  uint64_t requestTag;

  Request *request;

  LPN lpn;

  // Host-side DMA address
  uint64_t offset;  //!< Offset in DMA Tag
  uint32_t length;  //!< Length in DMA Tag

  bool allocate;  //!< Used in ICL, true when cacheline allocation is required
  bool miss;      //!< Used in ICL, true when miss

  uint32_t skipFront;
  uint32_t skipEnd;

  // Device-side DMA address
  uint64_t address;  //!< Physical address of internal DRAM

 public:
  SubRequest();
  SubRequest(uint64_t, Request *);
  SubRequest(uint64_t, Request *, LPN, uint64_t, uint32_t);
  SubRequest(const SubRequest &) = delete;
  SubRequest(SubRequest &&rhs) noexcept { *this = std::move(rhs); }

  SubRequest &operator=(const SubRequest &) = delete;
  SubRequest &operator=(SubRequest &&rhs) {
    if (this != &rhs) {
      this->requestTag = std::exchange(rhs.requestTag, 0);
      this->request = std::exchange(rhs.request, nullptr);
      this->lpn = std::exchange(rhs.lpn, 0);
      this->offset = std::exchange(rhs.offset, 0);
      this->length = std::exchange(rhs.length, 0);
      this->allocate = std::exchange(rhs.allocate, false);
      this->miss = std::exchange(rhs.miss, false);
      this->address = std::exchange(rhs.address, 0);
    }

    return *this;
  }

  inline void setDRAMAddress(uint64_t addr) { address = addr; }
  inline void setAllocate() { allocate = true; }
  inline void setMiss() { miss = true; }

  inline uint64_t getDRAMAddress() { return address; }
  inline uint64_t getTag() { return requestTag; }
  inline LPN getLPN() { return lpn; }
  inline bool getAllocate() { return allocate; }
  inline bool getMiss() { return miss; }

  inline Operation getOpcode() { return request->opcode; }
  inline uint64_t getParentTag() { return request->requestTag; }
  inline LPN getSLPN() { return request->slpn; }
  inline uint32_t getNLP() { return request->nlp; }

  inline uint64_t getTagForLog() {
    return requestTag - request->firstSubRequestTag;
  }

  inline bool isICLRequest() {
    // Return true if this request is generated by ICL, not host
    return request->eid == InvalidEventID;
  }

  inline uint32_t getSkipFront() { return skipFront; }
  inline uint32_t getSkipEnd() { return skipEnd; }

  /* Only for Flush, Trim and Format */
  inline uint64_t getOffset() { return offset; }
  inline uint32_t getLength() { return length; }

  /*
   * Buffer pointer for this SubRequest.
   *
   *  If read, this buffer is already filled with disk image data.
   *  If write, this buffer is allocated but empty -- will be overwritten by hil
   * in HIL::HIL::nvmCompletion and valid after HIL::HIL::dmaCompletion.
   */
  inline uint8_t *getBuffer() {
    return request->buffer ? request->buffer + offset : nullptr;
  }

  void createCheckpoint(std::ostream &) const noexcept;
  void restoreCheckpoint(std::istream &, HIL *) noexcept;
};

}  // namespace SimpleSSD::HIL

#endif
