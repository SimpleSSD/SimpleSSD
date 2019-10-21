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
  struct BlockMetadata {
    PPN blockID;
    uint64_t erasedCount;

    BlockMetadata() : blockID(InvalidPPN), erasedCount(0) {}
    BlockMetadata(PPN id) : blockID(id), erasedCount(0) {}
    BlockMetadata(PPN id, uint64_t e) : blockID(id), erasedCount(e) {}
  };

  uint64_t parallelism;
  uint64_t totalSuperblock;

  PPN lastAllocated;             // Used for pMapper->initialize
  BlockMetadata *inUseBlockMap;  // Allocated free blocks

  uint64_t freeBlockCount;               // Shortcut
  std::list<BlockMetadata> *freeBlocks;  // Free blocks sorted
  std::list<BlockMetadata> fullBlocks;   // Sorted in erased count

  Config::VictimSelectionMode selectionMode;
  Config::GCBlockReclaimMode countMode;
  float gcThreshold;
  float blockRatio;
  uint64_t blockCount;
  uint64_t dchoice;

  std::random_device rd;
  std::mt19937 mtengine;

 public:
  BasicAllocator(ObjectData &, Mapping::AbstractMapping *);
  ~BasicAllocator();

  void initialize(Parameter *) override;

  CPU::Function allocateBlock(PPN &) override;
  PPN getBlockAt(PPN) override;

  bool checkGCThreshold() override;
  void getVictimBlocks(std::deque<PPN> &, Event) override;
  void reclaimBlocks(std::vector<SubCommand> &, Event) override;

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::FTL::BlockAllocator

#endif
