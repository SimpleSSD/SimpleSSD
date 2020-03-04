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
  uint32_t pageSize;
  uint32_t parallelismLevel[4];  //!< Parallelism group list
  uint64_t parallelism;
  uint8_t superpageLevel;  //!< Number of levels (1~N) included in superpage
  uint64_t superpage;
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

  Operation opcode;
  Response result;

  // Current request information (Invalid when Trim/Format)
  LPN lpn;
  PPN ppn;

  // Full request information (Or current request info when Trim/Format)
  LPN slpn;
  uint32_t nlp;

  Event event;
  uint64_t data;

 public:
  Request(Event, uint64_t);
  Request(Event, HIL::SubRequest *);

  inline uint64_t getTag() { return tag; }

  inline Operation getOperation() { return opcode; }
  inline Response getResponse() { return result; }

  inline LPN getLPN() { return lpn; }
  inline PPN getPPN() { return ppn; }

  inline LPN getSLPN() { return slpn; }
  inline uint32_t getNLP() { return nlp; }

  inline Event getEvent() { return event; }
  inline uint64_t getEventData() { return data; }

  inline void setResponse(Response r) { result = r; }

  inline void setLPN(LPN l) { lpn = l; }
  inline void setPPN(PPN p) { ppn = p; }

  inline void setSLPN(LPN s) { slpn = s; }
  inline void setNLP(uint32_t l) { nlp = l; }

  uint32_t counter;

  void createCheckpoint(std::ostream &out) const noexcept;
  void restoreCheckpoint(std::istream &in, ObjectData &object) noexcept;
};

struct CopyList {
  PPN blockID;
  uint64_t eraseTag;

  std::vector<uint64_t>::iterator iter;
  std::vector<uint64_t> commandList;

  void resetIterator() { iter = commandList.begin(); }

  bool isEnd() { return iter == commandList.end(); }

  void createCheckpoint(std::ostream &out) const {
    BACKUP_SCALAR(out, blockID);

    uint64_t size = commandList.size();
    BACKUP_SCALAR(out, size);

    for (auto &iter : commandList) {
      BACKUP_SCALAR(out, iter);
    }

    size = iter - commandList.begin();
    BACKUP_SCALAR(out, size);
  }

  void restoreCheckpoint(std::istream &in) {
    RESTORE_SCALAR(in, blockID);

    uint64_t size;
    RESTORE_SCALAR(in, size);

    commandList.reserve(size);

    for (uint64_t i = 0; i < size; i++) {
      uint64_t tag;

      RESTORE_SCALAR(in, tag);

      commandList.emplace_back(tag);
    }

    RESTORE_SCALAR(in, size);
    iter = commandList.begin() + size;
  }
};

}  // namespace SimpleSSD::FTL

#endif
