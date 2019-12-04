// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_FTL_BASE_VLFTL_HH__
#define __SIMPLESSD_FTL_BASE_VLFTL_HH__

#include "ftl/base/basic_ftl.hh"

namespace SimpleSSD::FTL {

class VLFTL : public BasicFTL {
 private:
  bool mergeTriggered;
  uint64_t mergeTag;

  virtual inline void triggerGC() override;

  Event eventDoMerge;
  void merge_trigger();

  Event eventMergeReadDone;
  void merge_readDone();

  Event eventMergeWriteDone;
  void merge_writeDone();

 public:
  VLFTL(ObjectData &, FIL::FIL *, Mapping::AbstractMapping *,
        BlockAllocator::AbstractAllocator *);

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::FTL

#endif
