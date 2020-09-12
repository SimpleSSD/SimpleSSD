// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 *         Junhyeok Jang <jhjang@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_FTL_GC_NAIVE_HH__
#define __SIMPLESSD_FTL_GC_NAIVE_HH__

#include "ftl/gc/abstract_gc.hh"

namespace SimpleSSD::FTL::GC {

enum class State : uint32_t {
  /* Idle states */
  Idle,        // GC is not triggered
  Pauesd,      // GC has been suepended

  /* Active states */
  Foreground,  // GC triggered as foreground
  Background,  // GC triggered as background
};

class NaiveGC : public AbstractGC {
 protected:
  Log::DebugID logid;
  State state;

  uint32_t superpage;
  uint32_t pageSize;
  uint64_t bufferBaseAddress;
  uint64_t beginAt;

  std::deque<CopyContext> blockList;

  struct {
    uint64_t fgcCount;        // Foreground GC invoked count
    uint64_t bgcCount;        // Background GC invoked count
    uint64_t gcErasedBlocks;  // Erased blocks
    uint64_t gcCopiedPages;   // Copied pages

    // User-visible GC time

    uint64_t avg_penalty;
    uint64_t min_penalty;
    uint64_t max_penalty;
    uint64_t penalty_count;
  } stat;

  // For penalty calculation
  uint64_t firstRequestArrival;

  inline uint64_t makeBufferAddress(uint32_t superpageIndex,
                                    uint32_t pageIndex) {
    return bufferBaseAddress +
           (superpageIndex * superpage + pageIndex) * pageSize;
  }

  Event eventTrigger;
  virtual void gc_trigger();

  Event eventStart;
  virtual void gc_start(uint64_t);

  Event eventDoRead;
  virtual void gc_doRead(uint64_t, uint64_t);

  Event eventDoTranslate;
  virtual void gc_doTranslate(uint64_t, uint64_t);

  Event eventDoWrite;
  virtual void gc_doWrite(uint64_t, uint64_t);

  Event eventDoErase;
  virtual void gc_doErase(uint64_t, uint64_t);

  Event eventDone;
  virtual void gc_done(uint64_t, uint64_t);

 public:
  NaiveGC(ObjectData &, FTLObjectData &, FIL::FIL *);
  virtual ~NaiveGC();

  void initialize() override;

  void triggerForeground() override;
  void requestArrived(bool, uint32_t) override;

  bool checkWriteStall() override;

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::FTL::GC

#endif
