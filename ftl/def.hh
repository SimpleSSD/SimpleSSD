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
#include <unordered_map>

#include "hil/request.hh"
#include "sim/event.hh"
#include "sim/object.hh"
#include "util/bitset.hh"

namespace SimpleSSD::FTL {

class FTL;

typedef struct {
  uint64_t totalPhysicalBlocks;
  uint64_t totalPhysicalPages;
  uint64_t totalLogicalBlocks;
  uint64_t totalLogicalPages;
  uint32_t parallelismLevel[4];  //!< Parallelism group list
  uint64_t parallelism;
  uint64_t superpage;
  uint32_t pageSize;
  uint8_t superpageLevel;  //!< Number of levels (1~N) included in superpage
} Parameter;

enum class Operation : uint8_t {
  None,
  Read,
  Write,
  Trim,
  Format,
};

enum class Response : uint8_t {
  Success,
  Unwritten,
  FormatInProgress,
  ReadECCFail,
  WriteFail,
};

class Request {
 private:
  friend FTL;

  uint64_t tag;

  // Current request information (Invalid when Trim/Format)
  LPN lpn;  //!< Requested LPN or stored LPN in spare area
  PPN ppn;  //!< Translated PPN

  uint32_t offset;  //!< Byte offset in current page
  uint32_t length;  //!< Byte length in current page

  // Full request information (Or current request info when Trim/Format)
  LPN slpn;      //!< Starting LPN of parent request
  uint32_t nlp;  //!< Number of pages in parent request

  Operation opcode;
  Response result;

  Event event;    //!< Completion event
  uint64_t data;  //!< Tag of HIL::Request

  uint64_t address;  //!< Physical address of internal DRAM

 public:
  Request(Event, uint64_t);
  Request(Event, HIL::SubRequest *);
  Request(Operation, LPN, uint32_t, uint32_t, LPN, uint32_t, Event, uint64_t);

  inline uint64_t getTag() { return tag; }

  inline Operation getOperation() { return opcode; }
  inline Response getResponse() { return result; }

  inline LPN getLPN() { return lpn; }
  inline PPN getPPN() { return ppn; }

  inline LPN getSLPN() { return slpn; }
  inline uint32_t getNLP() { return nlp; }

  inline uint32_t getOffset() { return offset; }
  inline uint32_t getLength() { return length; }

  inline Event getEvent() { return event; }
  inline uint64_t getEventData() { return data; }

  inline uint64_t getDRAMAddress() { return address; }

  inline void setResponse(Response r) { result = r; }

  inline void setLPN(LPN l) { lpn = l; }
  inline void setPPN(PPN p) { ppn = p; }

  inline void setDRAMAddress(uint64_t addr) { address = addr; };

  uint32_t counter;

  void createCheckpoint(std::ostream &out) const noexcept;
  void restoreCheckpoint(std::istream &in, ObjectData &object) noexcept;
};
// list of request targeting same superpage
using SuperRequest = std::vector<Request *>;

struct CopyContext {
  PPN blockID;
  uint64_t eraseTag;

  std::vector<SuperRequest>::iterator iter;
  std::vector<SuperRequest> list;

  void resetIterator() { iter = list.begin(); }
  bool isEnd() { return iter == list.end(); }

  CopyContext(): blockID(InvalidPPN), eraseTag(0) {}

  void createCheckpoint(std::ostream &out) const {
    BACKUP_SCALAR(out, blockID);

    uint64_t size = list.size();
    BACKUP_SCALAR(out, size);

    for (auto &iter : list) {
      BACKUP_SCALAR(out, iter);
    }

    size = iter - list.begin();
    BACKUP_SCALAR(out, size);
  }

  void restoreCheckpoint(std::istream &in) {
    RESTORE_SCALAR(in, blockID);

    uint64_t size;
    RESTORE_SCALAR(in, size);

    list.reserve(size);

    for (uint64_t i = 0; i < size; i++) {
      uint64_t tag;

      RESTORE_SCALAR(in, tag);

      list.emplace_back(tag);
    }

    RESTORE_SCALAR(in, size);
    iter = list.begin() + size;
  }
};

}  // namespace SimpleSSD::FTL

#endif
