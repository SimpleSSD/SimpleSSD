// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 *         Junhyeok Jang <jhjang@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_FTL_MAPPING_PAGE_LEVEL_MAPPING_HH__
#define __SIMPLESSD_FTL_MAPPING_PAGE_LEVEL_MAPPING_HH__

#include "ftl/mapping/abstract_mapping.hh"

namespace SimpleSSD::FTL::Mapping {

class PageLevelMapping : public AbstractMapping {
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
    return tableBaseAddress + static_cast<uint64_t>(lpn) * entrySize;
  }

  inline uint64_t makeMetadataAddress(PBN block) {
    return metadataBaseAddress +
           static_cast<uint32_t>(block) * metadataEntrySize;
  }

 public:
  PageLevelMapping(ObjectData &);
  ~PageLevelMapping();

  void initialize(AbstractFTL *, BlockAllocator::AbstractAllocator *) override;

  uint64_t getPageUsage(LPN, uint64_t) override;

  uint32_t getValidPages(PPN, uint64_t) override;
  uint64_t getAge(PPN, uint64_t) override;

  void readMapping(Request *, Event) override;
  void writeMapping(Request *, Event) override;
  void invalidateMapping(Request *, Event) override;

  void getMappingSize(uint64_t *, uint64_t * = nullptr) override;

  void getCopyContext(CopyContext &, Event) override;
  void markBlockErased(PPN) override;

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::FTL::Mapping

#endif
