// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_FTL_ALLOCATOR_BASIC_ALLOCATOR_HH__
#define __SIMPLESSD_FTL_ALLOCATOR_BASIC_ALLOCATOR_HH__

#include <unordered_map>

#include "ftl/allocator/abstract_allocator.hh"

namespace SimpleSSD::FTL::BlockAllocator {

class BasicAllocator : public AbstractAllocator {
 protected:
  PPN *inUseBlockMap;  // Allocated free blocks

  std::list<PPN> *freeBlocks;  // Unallocated free blocks

 private:
  std::unordered_map<PPN, uint64_t> erasedCount;  // Erase count

 public:
  BasicAllocator(ObjectData &);
  ~BasicAllocator();

  void initialize(Parameter *) override;

  PPN allocateBlock(PPN, Event) override;

  void getVictimBlocks(std::deque<PPN> &, Event) override;
  void reclaimBlocks(std::deque<PPN> &, Event) override;

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::FTL::BlockAllocator

#endif
