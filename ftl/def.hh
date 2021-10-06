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
class AbstractFTL;
struct CopyContext;

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
    return static_cast<uint32_t>(ppn / totalPhysicalBlocks);
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
    return static_cast<uint32_t>(ppn % superpage);
  }

  //! Get SuperpageIndex from Logical Page Number
  inline uint32_t getSuperpageIndexFromLPN(LPN lpn) const {
    return static_cast<uint32_t>(lpn % superpage);
  }

  //! Get PSBN from PSPN
  inline PSBN getPSBNFromPSPN(PSPN pspn) const {
    return static_cast<PSBN>(pspn % (totalPhysicalBlocks / superpage));
  }

  //! Get PageIndex from PSPN
  inline uint32_t getPageIndexFromPSPN(PSPN pspn) const {
    return static_cast<uint32_t>(pspn / (totalPhysicalBlocks / superpage));
  }

  //! Make PSPN from PSBN and PageIndex
  inline PSPN makePSPN(PSBN psbn, uint32_t pageIndex) const {
    return static_cast<PSPN>(psbn +
                             pageIndex * (totalPhysicalBlocks / superpage));
  }

  //! Make PPN from PSBN, SuperpageIndex and PageIndex
  inline PPN makePPN(PSBN psbn, uint32_t superpageIndex,
                     uint32_t pageIndex) const {
    return static_cast<PPN>(psbn * superpage + pageIndex * totalPhysicalBlocks +
                            superpageIndex);
  }

  //! Make PPN from PSPN and SuperpageIndex
  inline PPN makePPN(PSPN pspn, uint32_t superpageIndex) const {
    return static_cast<PPN>(pspn * superpage + superpageIndex);
  }

  //! Make LPN from LSPN and SuperpageIndex
  inline LPN makeLPN(LSPN lspn, uint32_t superpageIndex) const {
    return static_cast<LPN>(lspn * superpage + superpageIndex);
  }

  //! Get parallelism index from PBN
  inline uint32_t getParallelismIndexFromPBN(PBN pbn) const {
    return static_cast<uint32_t>(pbn % parallelism);
  }

  //! Get parallelism index from PSBN
  inline uint32_t getParallelismIndexFromPSBN(PSBN psbn) const {
    return static_cast<uint32_t>(psbn % (parallelism / superpage));
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
  friend CopyContext;

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

  void createCheckpoint(std::ostream &) const noexcept;
  void restoreCheckpoint(std::istream &, ObjectData &) noexcept;
};

// list of request targeting same superpage
using SuperRequest = std::vector<Request *>;

void backupSuperRequest(std::ostream &, const SuperRequest &) noexcept;
void restoreSuperRequest(std::istream &, SuperRequest &,
                         AbstractFTL *) noexcept;

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

  inline void push_back(ReadModifyWriteContext *val) {
    if (last == nullptr) {
      next = val;
      last = val;
    }
    else {
      last->next = val;
      last = val;
    }
  }

  void createCheckpoint(std::ostream &) const noexcept;
  void restoreCheckpoint(std::istream &, AbstractFTL *) noexcept;
};

struct PageContext {
  Request request;  // We need LPN only, but just use full-sized Request struct

  uint32_t pageIndex;
  uint64_t beginAt;

  PageContext() : request(PPN()), pageIndex(0), beginAt(0) {}
  PageContext(uint32_t idx) : request(PPN()), pageIndex(idx), beginAt(0) {}
  PageContext(LPN lpn, PPN ppn, uint32_t idx)
      : request(ppn), pageIndex(idx), beginAt(0) {
    request.setLPN(lpn);
  }
};

struct CopyContext {
  PSBN blockID;

  std::vector<PageContext> copyList;

  uint32_t pageReadIndex;
  uint32_t pageWriteIndex;

  uint32_t readCounter;
  uint32_t writeCounter;

  uint64_t beginAt;

  CopyContext()
      : pageReadIndex(0),
        pageWriteIndex(0),
        readCounter(0),
        writeCounter(0),
        beginAt(0) {}
  CopyContext(PSBN b)
      : blockID(b),
        pageReadIndex(0),
        pageWriteIndex(0),
        readCounter(0),
        writeCounter(0),
        beginAt(0) {}

  void createCheckpoint(std::ostream &out) const noexcept {
    BACKUP_SCALAR(out, blockID);

    BACKUP_STL(out, copyList, iter, {
      BACKUP_SCALAR(out, iter.request.lpn);
      BACKUP_SCALAR(out, iter.request.ppn);
      BACKUP_SCALAR(out, iter.pageIndex);
      BACKUP_SCALAR(out, iter.beginAt);
    });

    BACKUP_SCALAR(out, readCounter);
    BACKUP_SCALAR(out, writeCounter);
    BACKUP_SCALAR(out, beginAt);
  }

  void restoreCheckpoint(std::istream &in) noexcept {
    RESTORE_SCALAR(in, blockID);

    RESTORE_STL_RESERVE(in, copyList, i, {
      LPN lpn;
      PPN ppn;
      uint32_t idx;

      RESTORE_SCALAR(in, lpn);
      RESTORE_SCALAR(in, ppn);
      RESTORE_SCALAR(in, idx);

      auto &ret = copyList.emplace_back(PageContext(lpn, ppn, idx));

      RESTORE_SCALAR(in, ret.beginAt);
    });

    RESTORE_SCALAR(in, readCounter);
    RESTORE_SCALAR(in, writeCounter);
    RESTORE_SCALAR(in, beginAt);
  }
};

struct BlockMetadata {
  /* Necessary metadata */

  Bitset validPages;
  uint32_t nextPageToWrite;

  /* Metadata for background operations */

  uint32_t erasedCount;          // P/E Cycle
  uint32_t readCountAfterErase;  // For read reclaim and read refresh
  uint32_t writeCountAfterErase;
  uint64_t insertedAt;  // For Cost-benefit GC

  BlockMetadata()
      : nextPageToWrite(0),
        erasedCount(0),
        readCountAfterErase(0),
        writeCountAfterErase(0),
        insertedAt(0) {}
  BlockMetadata(uint32_t pages)
      : validPages(pages),
        nextPageToWrite(0),
        erasedCount(0),
        readCountAfterErase(0),
        writeCountAfterErase(0),
        insertedAt(0) {}

  /* Helper functions for metadata updates */

  inline void markAsErased() { erasedCount++, readCountAfterErase = 0; }
  inline void markAsRead() { readCountAfterErase++; }
  inline void markAsWrite() { writeCountAfterErase++; }
  inline bool isEmpty() { return nextPageToWrite == 0; }
  inline bool isFull() { return nextPageToWrite == validPages.size(); }
  inline bool isOpen() { return nextPageToWrite > 0 && !isFull(); }

  /* Helper functions for memory access insertion */

  inline constexpr uint32_t offsetofPageIndex() { return 0; }
  inline constexpr uint32_t offsetofErasedCount() { return 4; }
  inline constexpr uint32_t offsetofReadCount() { return 8; }
  inline constexpr uint32_t offsetofWriteCount() { return 12; }
  inline uint32_t offsetofBitmap(uint32_t index) { return 16 + index / 8; }
  inline uint32_t sizeofMetadata() {
    return 16 + DIVCEIL(validPages.size(), 8);
  }

  /* Helper functions for checkpointing API */

  void createCheckpoint(std::ostream &out) const noexcept {
    validPages.createCheckpoint(out);

    BACKUP_SCALAR(out, nextPageToWrite);
    BACKUP_SCALAR(out, erasedCount);
    BACKUP_SCALAR(out, readCountAfterErase);
    BACKUP_SCALAR(out, insertedAt);
  }

  void restoreCheckpoint(std::istream &in) noexcept {
    validPages.restoreCheckpoint(in);

    RESTORE_SCALAR(in, nextPageToWrite);
    RESTORE_SCALAR(in, erasedCount);
    RESTORE_SCALAR(in, readCountAfterErase);
    RESTORE_SCALAR(in, insertedAt);
  }
};

}  // namespace SimpleSSD::FTL

#endif
