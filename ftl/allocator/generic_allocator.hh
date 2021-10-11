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

#include "ftl/allocator/abstract_allocator.hh"

namespace SimpleSSD::FTL::BlockAllocator {

class GenericAllocator : public AbstractAllocator {
 protected:
  const uint64_t totalSuperblock;
  const uint32_t superpage;
  const uint32_t parallelism;

  std::vector<BlockMetadata> blockMetadata;
  const uint64_t metadataBaseAddress;
  const uint64_t metadataEntrySize;

  inline uint64_t makeMetadataAddress(PSBN block) {
    return metadataBaseAddress + block * metadataEntrySize;
  }

  struct AllocationMetadata {
    PSBN inUse;  // Allocate freeblock at current index
    PSBN inUseHighPE;

    std::list<PSBN> freeBlocks;  // Free blocks sorted in erased count
    std::list<PSBN> fullBlocks;  // Full blocks sorted in erased count;

    inline void createCheckpoint(std::ostream &out) const noexcept {
      BACKUP_SCALAR(out, inUse);
      BACKUP_SCALAR(out, inUseHighPE);

      BACKUP_STL(out, freeBlocks, iter, BACKUP_SCALAR(out, iter));
      BACKUP_STL(out, fullBlocks, iter, BACKUP_SCALAR(out, iter));
    }

    inline void restoreCheckpoint(std::istream &in) noexcept {
      RESTORE_SCALAR(in, inUse);
      RESTORE_SCALAR(in, inUseHighPE);

      RESTORE_STL(in, i, {
        auto &iter = freeBlocks.emplace_back(PSBN{});

        RESTORE_SCALAR(in, iter);
      });

      RESTORE_STL(in, i, {
        auto &iter = fullBlocks.emplace_back(PSBN{});

        RESTORE_SCALAR(in, iter);
      });
    }
  };

  uint32_t lastErased;     // Used for victim selection
  uint32_t lastAllocated;  // Used for pMapper->initialize

  uint64_t freeBlockCount;  // Free block count shortcut
  uint64_t fullBlockCount;  // Full block count shortcut

  std::vector<AllocationMetadata> sortedBlockList;

  float fgcThreshold;
  float bgcThreshold;

 public:
  GenericAllocator(ObjectData &, FTLObjectData &);
  virtual ~GenericAllocator();

  void initialize() override;

  BlockMetadata &getBlockMetadata(const PSBN &psbn) noexcept override {
    panic_if(psbn >= totalSuperblock, "Block ID out-of-range.");

    return blockMetadata.at(psbn);
  }

  uint64_t getMemoryAddressOfBlockMetadata(const PSBN &psbn) noexcept override {
    return makeMetadataAddress(psbn);
  }

  CPU::Function allocateFreeBlock(PSBN &, AllocationStrategy) override;
  PSBN getFreeBlockAt(uint32_t, AllocationStrategy) noexcept override;

  bool checkForegroundGCThreshold() noexcept override;
  bool checkBackgroundGCThreshold() noexcept override;
  void getVictimBlock(CopyContext &, AbstractVictimSelection *, Event,
                      uint64_t) override;
  void reclaimBlock(PSBN, Event, uint64_t) override;

  void getPageStatistics(uint64_t &, uint64_t &) noexcept override;

  std::list<PSBN> &getBlockListAtParallelismIndex(uint32_t) noexcept override;

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::FTL::BlockAllocator

#endif
