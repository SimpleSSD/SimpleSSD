// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 *         Junhyeok Jang <jhjang@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_FTL_ALLOCATOR_GENERIC_ALLOCATOR_HH__
#define __SIMPLESSD_FTL_ALLOCATOR_GENERIC_ALLOCATOR_HH__

#include <list>
#include <random>

#include "ftl/allocator/abstract_allocator.hh"

namespace SimpleSSD::FTL::BlockAllocator {

class GenericAllocator : public AbstractAllocator {
 protected:
  using BlockSelection = std::function<CPU::Function(uint64_t, CopyContext &)>;

  uint64_t totalSuperblock;
  uint32_t superpage;
  uint32_t parallelism;

  uint32_t *eraseCountList;

  uint32_t lastErased;     // Used for victim selection
  uint32_t lastAllocated;  // Used for pMapper->initialize
  PSBN *inUseBlockMap;     // Allocated free blocks

  uint64_t freeBlockCount;      // Free block count shortcut
  uint64_t fullBlockCount;      // Full block count shortcut
  std::list<PSBN> *freeBlocks;  // Free blocks sorted in erased count
  std::list<PSBN> *fullBlocks;  // Full blocks sorted in erased count

  Config::VictimSelectionMode selectionMode;
  float fgcThreshold;
  float bgcThreshold;
  uint64_t dchoice;

  std::random_device rd;
  std::mt19937 mtengine;

  BlockSelection victimSelectionFunction;

  CPU::Function randomVictimSelection(uint64_t, CopyContext &);
  CPU::Function greedyVictimSelection(uint64_t, CopyContext &);
  CPU::Function costbenefitVictimSelection(uint64_t, CopyContext &);
  CPU::Function dchoiceVictimSelection(uint64_t, CopyContext &);

 public:
  GenericAllocator(ObjectData &, FTLObjectData &);
  virtual ~GenericAllocator();

  void initialize() override;

  CPU::Function allocateBlock(PSBN &) override;
  PSBN getBlockAt(uint32_t) override;

  bool checkForegroundGCThreshold() override;
  bool checkBackgroundGCThreshold() override;
  bool checkWriteStall() override;
  void getVictimBlocks(CopyContext &, Event) override;
  void reclaimBlocks(PSBN, Event) override;

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::FTL::BlockAllocator

#endif
