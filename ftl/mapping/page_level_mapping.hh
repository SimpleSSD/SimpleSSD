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

  ReadEntryFunction readTableEntry;
  WriteEntryFunction writeTableEntry;
  ParseEntryFunction parseTableEntry;
  MakeEntryFunction makeTableEntry;

  CPU::Function readMappingInternal(LSPN, PSPN &);
  CPU::Function writeMappingInternal(LSPN, PSPN &, bool = false);
  CPU::Function invalidateMappingInternal(LSPN, PSPN &);

  inline uint64_t makeTableAddress(LSPN lspn) {
    return tableBaseAddress + lspn * entrySize;
  }

 public:
  PageLevelMapping(ObjectData &, FTLObjectData &);
  ~PageLevelMapping();

  void initialize() override;

  uint64_t getPageUsage(LPN, uint64_t) override;

  void readMapping(Request *, Event) override;
  void writeMapping(Request *, Event) override;
  void writeMapping(LSPN, PSPN &) override;
  void invalidateMapping(Request *, Event) override;

  void getMappingSize(uint64_t *, uint64_t * = nullptr) override;

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::FTL::Mapping

#endif
