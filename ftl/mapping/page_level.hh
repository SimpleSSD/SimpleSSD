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

  BlockMetadata *blockMetadata;
  uint64_t metadataBaseAddress;
  uint64_t metadataEntrySize;

  ReadEntryFunction readTableEntry;
  WriteEntryFunction writeTableEntry;
  ParseEntryFunction parseTableEntry;
  MakeEntryFunction makeTableEntry;

  void physicalSuperPageStats(uint64_t &, uint64_t &);
  CPU::Function readMappingInternal(LPN, PPN &);
  CPU::Function writeMappingInternal(LPN, PPN &, bool = false);
  CPU::Function invalidateMappingInternal(LPN, PPN &);

  inline uint64_t makeTableAddress(LPN lpn) {
    return tableBaseAddress + lpn * entrySize;
  }

  inline uint64_t makeMetadataAddress(PPN block) {
    return metadataBaseAddress + block * metadataEntrySize;
  }

  /* Superpage/block related helper APIs */
  inline PPN getSuperblockFromSPPN(PPN sppn) {
    return sppn % totalPhysicalSuperBlocks;
  }

  inline PPN getSuperpageFromSPPN(PPN sppn) {
    return sppn / totalPhysicalSuperBlocks;
  }

  inline PPN makeSPPN(PPN sblk, PPN sp) {
    return sblk + sp * totalPhysicalSuperBlocks;
  }

 public:
  PageLevel(ObjectData &);
  ~PageLevel();

  void initialize(AbstractFTL *, BlockAllocator::AbstractAllocator *) override;

  LPN getPageUsage(LPN, LPN) override;

  uint32_t getValidPages(PPN, uint64_t) override;
  uint64_t getAge(PPN, uint64_t) override;

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
