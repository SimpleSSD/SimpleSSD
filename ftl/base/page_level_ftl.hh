// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 *         Junhyeok Jang <jhjang@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_FTL_BASE_PAGE_LEVEL_FTL_HH__
#define __SIMPLESSD_FTL_BASE_PAGE_LEVEL_FTL_HH__

#include "ftl/base/abstract_ftl.hh"

namespace SimpleSSD::FTL {

class PageLevelFTL : public AbstractFTL {
 protected:
  uint32_t pageSize;

  bool mergeReadModifyWrite;  // if True, merge RMW requests with same PPN

  uint64_t minMappingSize;

  // Pending request
  uint64_t pendingListBaseAddress;
  SuperRequest pendingList;

  std::list<SuperRequest> writeList;

  std::list<SuperRequest>::iterator getWriteContext(uint64_t);

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
        : alignedBegin(InvalidLPN),
          chunkBegin(InvalidLPN),
          next(nullptr),
          last(nullptr),
          writePending(false),
          counter(0),
          beginAt(0) {}
    ReadModifyWriteContext(uint64_t size)
        : alignedBegin(InvalidLPN),
          chunkBegin(InvalidLPN),
          list(size, nullptr),
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

  std::unordered_map<uint64_t, ReadModifyWriteContext> rmwList;

  // list of stalled request <tag, writeRequestType>
  std::deque<Request *> stalledRequests;

  struct GCContext {
    bool inProgress;
    std::vector<PPN> victimSBlockList;
    CopyContext copyctx;
    uint64_t bufferBaseAddress;

    uint64_t beginAt;
    uint64_t erasedBlocks;

    GCContext() : inProgress(false), beginAt(0) {}

    void init(uint64_t now) {
      inProgress = true;
      beginAt = now;
      victimSBlockList.clear();
      erasedBlocks = 0;
    }
  };
  GCContext gcctx;

  std::unordered_map<uint64_t, ReadModifyWriteContext>::iterator getRMWContext(
      uint64_t);

  inline LPN getAlignedLPN(LPN lpn) {
    return lpn / minMappingSize * minMappingSize;
  }

  // Statistics
  struct {
    uint64_t rmwCount;         // Total number of RMW operation
    uint64_t rmwMerged;        // Total number of merged RMW operation
    uint64_t rmwReadPages;     // Read pages in RMW
    uint64_t rmwWrittenPages;  // Written pages in RMW
    uint64_t gcCount;          // GC invoked count
    uint64_t gcErasedBlocks;   // Erased blocks
    uint64_t gcCopiedPages;    // Copied pages
  } stat;

  virtual inline void triggerGC() {
    if (pAllocator->checkGCThreshold() && !gcctx.inProgress) {
      gcctx.inProgress = true;
      scheduleNow(eventGCTrigger);
    }
  }

  Event eventReadSubmit;
  void read_submit(uint64_t);

  Event eventReadDone;
  void read_done(uint64_t);

  Event eventWriteSubmit;
  void write_submit(uint64_t);

  Event eventWriteDone;
  void write_done(uint64_t);

  Event eventPartialReadSubmit;
  void rmw_readSubmit(uint64_t, uint64_t);

  Event eventPartialReadDone;
  void rmw_readDone(uint64_t, uint64_t);

  Event eventPartialWriteSubmit;
  void rmw_writeSubmit(uint64_t, uint64_t);

  Event eventPartialWriteDone;
  void rmw_writeDone(uint64_t, uint64_t);

  Event eventInvalidateSubmit;
  void invalidate_submit(uint64_t, uint64_t);

  Event eventGCTrigger;
  void gc_trigger(uint64_t);

  Event eventGCSetNextVictimBlock;
  void gc_setNextVictimBlock(uint64_t);

  Event eventGCReadSubmit;
  void gc_readSubmit();

  Event eventGCReadDone;
  void gc_readDone(uint64_t);

  Event eventGCWriteSubmit;
  void gc_writeSubmit(uint64_t);

  Event eventGCWriteDone;
  void gc_writeDone(uint64_t, uint64_t);

  Event eventGCEraseSubmit;
  void gc_eraseSubmit();

  Event eventGCEraseDone;
  void gc_eraseDone(uint64_t);

  Event eventGCDone;
  void gc_done(uint64_t);

  void backup(std::ostream &, const SuperRequest &) const noexcept;
  void restore(std::istream &, SuperRequest &) noexcept;

 public:
  PageLevelFTL(ObjectData &, FTL *, FIL::FIL *, Mapping::AbstractMapping *,
               BlockAllocator::AbstractAllocator *);
  virtual ~PageLevelFTL();

  void read(Request *) override;
  void write(Request *) override;
  void invalidate(Request *) override;

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::FTL

#endif