// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 *         Junhyeok Jang <jhjang@camelab.org>
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

//! FTL parameter
struct Parameter {
  uint64_t totalPhysicalBlocks;
  uint64_t totalPhysicalPages;
  uint64_t totalLogicalBlocks;
  uint64_t totalLogicalPages;
  uint32_t parallelismLevel[4];  //!< Parallelism group list
  uint32_t parallelism;
  uint32_t superpage;  // pages per superpage
  uint32_t pageSize;
  uint8_t superpageLevel;  //!< Number of levels (1~N) included in superpage

  /*
   * Utility functions
   *
   * Conventions:
   *  PBN: Physical Block Number (Same as PPN, but page index field is zero)
   *  PSBN: Physical Superblock Number (Same as PSPN, but superpage index field
   *        is zero)
   *  PageIndex: Index of page in physical block
   *  SuperpageIndex: Index of page in SUPERPAGE (not superblock. Because Index
   *                  of superpage in superblock is same as PageIndex)
   *  ParallelismIndex: Index of parallelism (See homepage)
   *
   * Note:
   *  PPN == PSPN and PBN == PSBN when Parameter::superpage = 1.
   */

  //! Get PBN from PPN
  inline PBN getPBNFromPPN(PPN ppn) const {
    return static_cast<PBN>(ppn % totalPhysicalBlocks);
  }

  //! Get PageIndex from PPN
  inline uint32_t getPageIndexFromPPN(PPN ppn) const {
    return ppn / totalPhysicalBlocks;
  }

  //! Make PPN from PBN and PageIndex
  inline PPN makePPN(PBN pbn, uint32_t pageIndex) const {
    return static_cast<PPN>(pbn + pageIndex * totalPhysicalBlocks);
  }

  //! Get Physical Superpage Number from Physical Page Number
  inline PSPN getPSPNFromPPN(PPN ppn) const {
    return static_cast<PSPN>(ppn / superpage);
  }

  //! Get Logical Superpage Number from Logical Page Number
  inline LSPN getLSPNFromLPN(LPN lpn) const {
    return static_cast<LSPN>(lpn / superpage);
  }

  //! Get SuperpageIndex from Physical Page Number
  inline uint32_t getSuperpageIndexFromPPN(PPN ppn) const {
    return ppn % superpage;
  }

  //! Get SuperpageIndex from Logical Page Number
  inline uint32_t getSuperpageIndexFromLPN(LPN lpn) const {
    return lpn % superpage;
  }

  //! Get PSBN from PSPN
  inline PSBN getPSBNFromPSPN(PSPN pspn) const {
    return static_cast<PSBN>(pspn % (totalPhysicalBlocks / superpage));
  }

  //! Get PageIndex from PSPN
  inline uint32_t getPageIndexFromPSPN(PSPN pspn) const {
    return pspn / (totalPhysicalBlocks / superpage);
  }

  //! Make PSPN from PSBN and PageIndex
  inline PSPN makePSPN(PSBN psbn, uint32_t pageIndex) const {
    return static_cast<PSPN>(psbn +
                             pageIndex * (totalPhysicalBlocks / superpage));
  }

  //! Make PPN from PSBN, SuperpageIndex and PageIndex
  inline PPN makePPN(PSBN psbn, uint32_t pageIndex,
                     uint32_t superpageIndex) const {
    return static_cast<PPN>(psbn * superpage + pageIndex * totalPhysicalBlocks +
                            superpageIndex);
  }

  //! Make PPN from PSPN and SuperpageIndex
  inline PPN makePPN(PSPN pspn, uint32_t superpageIndex) const {
    return static_cast<PPN>(pspn * superpage + superpageIndex);
  }

  //! Get parallelism index from PBN
  inline uint32_t getParallelismIndexFromPBN(PBN pbn) const {
    return pbn % parallelism;
  }

  //! Get parallelism index from PSBN
  inline uint32_t getParallelismIndexFromPSBN(PSBN psbn) const {
    return psbn % (parallelism / superpage);
  }
};

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
  Request(PPN);

  inline uint64_t getTag() { return tag; }
  inline void setTag(uint64_t t) { tag = t; }

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

struct ReadModifyWriteContext {
  LPN alignedBegin;
  LPN chunkBegin;

  SuperRequest list;

  ReadModifyWriteContext *next;
  ReadModifyWriteContext *last;

  bool writePending;
  uint64_t counter;

  uint64_t beginAt;

  ReadModifyWriteContext()
      : next(nullptr),
        last(nullptr),
        writePending(false),
        counter(0),
        beginAt(0) {}
  ReadModifyWriteContext(uint64_t size)
      : list(size, nullptr),
        next(nullptr),
        last(nullptr),
        writePending(false),
        counter(0),
        beginAt(0) {}

  void push_back(ReadModifyWriteContext *val) {
    if (last == nullptr) {
      next = val;
      last = val;
    }
    else {
      last->next = val;
      last = val;
    }
  }
};

struct CopyContext {
  PSBN sblockID;

  std::vector<SuperRequest> list;
  std::vector<SuperRequest>::iterator iter;

  std::unordered_map<uint64_t, uint64_t> tag2ListIdx;

  uint64_t readCounter;  // count page read in progress
  // count page write in progress (shares index with list)
  std::vector<uint16_t> writeCounter;
  uint64_t copyCounter;   // count superpage copy in progress
  uint64_t eraseCounter;  // count block copy in progress

  uint64_t beginAt;

  CopyContext() = default;
  ~CopyContext();
  CopyContext(const CopyContext &) = delete;
  CopyContext &operator=(const CopyContext &) = delete;
  CopyContext(CopyContext &&rhs) noexcept : CopyContext() {
    *this = std::move(rhs);
  }
  CopyContext &operator=(CopyContext &&);

  void reset();
  inline void initIter() { iter = list.begin(); }
  inline bool isReadSubmitDone() { return iter == list.end(); }
  inline bool isReadDone() { return readCounter == 0; }
  inline bool iswriteDone(uint64_t listIndex) {
    return writeCounter.at(listIndex) == 0;
  }
  inline bool isEraseDone() { return eraseCounter == 0; }
  inline bool isCopyDone() { return copyCounter == 0; }
  void releaseList();

  void createCheckpoint(std::ostream &) const {}
  void restoreCheckpoint(std::istream &) {}
};

}  // namespace SimpleSSD::FTL

#endif
