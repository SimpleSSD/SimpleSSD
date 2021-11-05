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

class NaiveGC : public AbstractGC {
 protected:
  uint64_t beginAt;

  uint32_t fgcBlocksToErase;
  uint32_t bgcBlocksToErase;

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
    uint64_t affected_requests;
  } stat;

  // For penalty calculation
  uint64_t firstRequestArrival;

  inline void updatePenalty(uint64_t now) {
    if (firstRequestArrival < now) {
      auto penalty = now - firstRequestArrival;

      stat.penalty_count++;
      stat.avg_penalty += penalty;
      stat.min_penalty = MIN(stat.min_penalty, penalty);
      stat.max_penalty = MAX(stat.max_penalty, penalty);

      firstRequestArrival = std::numeric_limits<uint64_t>::max();
    }
  }

  uint32_t getParallelBlockCount();

  Event eventTrigger;
  virtual void trigger();

  void readPage(uint64_t, uint32_t) override;
  void done(uint64_t, uint32_t) override;

 public:
  NaiveGC(ObjectData &, FTLObjectData &, FIL::FIL *);
  virtual ~NaiveGC();

  void initialize(bool) override;

  void triggerForeground() override;
  void requestArrived(Request *) override;

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::FTL::GC

#endif
