// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_FTL_MAPPING_PAGE_LEVEL_HH__
#define __SIMPLESSD_FTL_MAPPING_PAGE_LEVEL_HH__

#include "ftl/mapping/abstract_mapping.hh"

namespace SimpleSSD::FTL::Mapping {

class PageLevel : public AbstractMapping {
 private:
  const uint64_t totalPhysicalSuperPages;
  const uint64_t totalPhysicalSuperBlocks;
  const uint64_t totalLogicalSuperPages;

  uint64_t entrySize;

  uint64_t tableBaseAddress;
  uint8_t *table;
  Bitset validEntry;

  BlockMetadata *blockMetadata;
  uint64_t metadataBaseAddress;
  uint64_t metadataEntrySize;

  inline uint64_t makeEntrySize() {
    uint64_t ret = 8;

    // Memory consumption optimization
    if (totalPhysicalSuperPages <= std::numeric_limits<uint16_t>::max()) {
      ret = 2;

      readEntry = [this](LPN lpn) {
        return (PPN)(*(uint16_t *)(table + (lpn << 1)));
      };
      writeEntry = [this](LPN lpn, PPN ppn) {
        *(uint16_t *)(table + (lpn << 1)) = (uint16_t)ppn;
      };
    }
    else if (totalPhysicalSuperPages <= std::numeric_limits<uint32_t>::max()) {
      ret = 4;

      readEntry = [this](LPN lpn) {
        return (PPN)(*(uint32_t *)(table + (lpn << 2)));
      };
      writeEntry = [this](LPN lpn, PPN ppn) {
        *(uint32_t *)(table + (lpn << 2)) = (uint32_t)ppn;
      };
    }
    else if (totalPhysicalSuperPages <= ((uint64_t)1ull << 48)) {
      uint64_t mask = (uint64_t)0x0000FFFFFFFFFFFF;
      ret = 6;

      readEntry = [this, mask](LPN lpn) {
        return ((PPN)(*(uint64_t *)(table + (lpn * 6)))) & mask;
      };
      writeEntry = [this](LPN lpn, PPN ppn) {
        memcpy(table + lpn * 6, &ppn, 6);
      };
    }
    else {
      ret = 8;

      readEntry = [this](LPN lpn) {
        return (PPN)(*(uint64_t *)(table + (lpn << 3)));
      };
      writeEntry = [this](LPN lpn, PPN ppn) {
        *(uint64_t *)(table + (lpn << 3)) = (uint64_t)ppn;
      };
    }

    return ret;
  }

  std::function<PPN(LPN)> readEntry;
  std::function<void(LPN, PPN)> writeEntry;

  void physicalSuperPageStats(uint64_t &, uint64_t &);
  CPU::Function readMappingInternal(LPN, PPN &, uint64_t);
  CPU::Function writeMappingInternal(LPN, PPN &, uint64_t, bool = false);
  CPU::Function invalidateMappingInternal(LPN, PPN &);

  inline uint64_t makeTableAddress(LPN lpn) {
    return tableBaseAddress + lpn * entrySize;
  }

  inline uint64_t makeMetadataAddress(PPN block) {
    return metadataBaseAddress + block * metadataEntrySize;
  }


 public:
  PageLevel(ObjectData &);
  ~PageLevel();

  void initialize(AbstractFTL *, BlockAllocator::AbstractAllocator *) override;

  LPN getPageUsage(LPN, LPN) override;

  uint32_t getValidPages(PPN) override;
  uint64_t getAge(PPN) override;

  void readMapping(Request *, Event) override;
  void writeMapping(Request *, Event) override;
  void invalidateMapping(Request *, Event) override;

  void getMappingSize(uint64_t *, uint64_t * = nullptr) override;

  void getCopyList(CopyList &, Event) override;
  void releaseCopyList(CopyList &) override;

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::FTL::Mapping

#endif
