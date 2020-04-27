// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_FTL_ALLOCATOR_BASIC_ALLOCATOR_HH__
#define __SIMPLESSD_FTL_ALLOCATOR_BASIC_ALLOCATOR_HH__

#include <list>
#include <random>

#include "ftl/allocator/abstract_allocator.hh"

namespace SimpleSSD::FTL::BlockAllocator {

class BasicAllocator : public AbstractAllocator {
 protected:
  using BlockSelection =
      std::function<CPU::Function(uint64_t, std::deque<PPN> &)>;

  uint64_t parallelism;
  uint64_t totalSuperblock;

  uint32_t *eraseCountList;

  PPN lastAllocated;   // Used for pMapper->initialize
  PPN *inUseBlockMap;  // Allocated free blocks

  uint64_t freeBlockCount;     // Free block count shortcut
  uint64_t fullBlockCount;     // Full block count shortcut
  std::list<PPN> *freeBlocks;  // Free blocks sorted in erased count
  std::list<PPN> *fullBlocks;  // Full blocks sorted in erased count

  Config::VictimSelectionMode selectionMode;
  float gcThreshold;
  uint64_t dchoice;

  std::random_device rd;
  std::mt19937 mtengine;

  BlockSelection victimSelectionFunction;

 public:
  BasicAllocator(ObjectData &, Mapping::AbstractMapping *);
  virtual ~BasicAllocator();

  void initialize(Parameter *) override;

  CPU::Function allocateBlock(PPN &, uint64_t) override;
  PPN getBlockAt(PPN, uint64_t) override;

  bool checkGCThreshold() override;
  void getVictimBlocks(std::deque<PPN> &, Event) override;
  void reclaimBlocks(PPN, Event) override;

  inline PPN getParallelismFromSPPN(PPN sppn) override {
    return sppn % parallelism;
  }

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::FTL::BlockAllocator

#endif
