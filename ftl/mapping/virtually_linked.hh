// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_FTL_MAPPING_VIRTUALLY_LINKED_HH__
#define __SIMPLESSD_FTL_MAPPING_VIRTUALLY_LINKED_HH__

#include "ftl/mapping/abstract_mapping.hh"
#include "util/bitset.hh"

namespace SimpleSSD::FTL::Mapping {

class VirtuallyLinked : public AbstractMapping {
 private:
  struct PartialTableEntry {
    LPN slpn;

    const uint32_t superpage;
    const uint32_t entrySize;

    uint8_t *data;
    Bitset valid;

    PartialTableEntry(LPN sl, uint32_t sp, uint32_t es)
        : slpn(sl), superpage(sp), entrySize(es), valid(sp) {
      data = (uint8_t *)calloc(sp, es);
    }
    PartialTableEntry(const PartialTableEntry &) = delete;
    PartialTableEntry(PartialTableEntry &&rhs) noexcept
        : slpn(rhs.slpn),
          superpage(rhs.superpage),
          entrySize(rhs.entrySize),
          data(nullptr) {
      if (this != &rhs) {
        free(data);

        data = std::exchange(rhs.data, nullptr);
        valid = std::move(rhs.valid);
      }
    }
    ~PartialTableEntry() { free(data); }

    inline bool isValid(uint32_t si) { return valid.test(si); }
    inline void setEntry(uint32_t si, PPN ppn) {
      valid.set(si);

      memcpy(data + entrySize * si, &ppn, entrySize);
    }
    inline void resetEntry(uint32_t si) { valid.reset(si); }
    inline PPN getEntry(uint32_t si) {
      PPN ppn = 0;

      memcpy(&ppn, data + entrySize * si, entrySize);

      return ppn;
    }
  };

  const uint64_t totalPhysicalSuperPages;
  const uint64_t totalPhysicalSuperBlocks;
  const uint64_t totalLogicalSuperPages;

  uint64_t entrySize;
  uint64_t pointerSize;

  uint8_t *table;
  Bitset validEntry;

  uint8_t *pointer;
  Bitset pointerValid;

  // No way to construct array without default constructor -> just use vector
  std::vector<PartialTableEntry> partialTable;
  float mergeBeginThreshold;
  float mergeEndThreshold;

  BlockMetadata *blockMetadata;

  uint16_t clock;

  inline uint64_t makeEntrySize() {
    uint64_t ret = 8;

    // Memory consumption optimization
    // We are using InvalidLPN/PPN as max value (exclude equal to)
    if (totalPhysicalSuperPages < std::numeric_limits<uint16_t>::max()) {
      ret = 2;

      readEntry = [this](LPN lpn) {
        return (PPN)(*(uint16_t *)(table + (lpn << 1)));
      };
      writeEntry = [this](LPN lpn, PPN ppn) {
        *(uint16_t *)(table + (lpn << 1)) = (uint16_t)ppn;
      };
    }
    else if (totalPhysicalSuperPages < std::numeric_limits<uint32_t>::max()) {
      ret = 4;

      readEntry = [this](LPN lpn) {
        return (PPN)(*(uint32_t *)(table + (lpn << 2)));
      };
      writeEntry = [this](LPN lpn, PPN ppn) {
        *(uint32_t *)(table + (lpn << 2)) = (uint32_t)ppn;
      };
    }
    else if (totalPhysicalSuperPages < ((uint64_t)1ull << 48)) {
      uint64_t mask = (uint64_t)0x0000FFFFFFFFFFFF;
      ret = 6;

      readEntry = [this, mask](LPN lpn) {
        return ((PPN)(*(uint64_t *)(table + (lpn * 6)))) | mask;
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

  inline uint64_t makePointerSize(uint64_t size) {
    uint64_t ret = 8;

    if (size < std::numeric_limits<uint16_t>::max()) {
      ret = 2;

      readPointer = [this](LPN lpn) {
        return (uint64_t)(*(uint16_t *)(table + (lpn << 1)));
      };
      writePointer = [this](LPN lpn, uint64_t ppn) {
        *(uint16_t *)(table + (lpn << 1)) = (uint16_t)ppn;
      };
    }
    else if (size < std::numeric_limits<uint32_t>::max()) {
      ret = 4;

      readPointer = [this](LPN lpn) {
        return (uint64_t)(*(uint32_t *)(table + (lpn << 2)));
      };
      writePointer = [this](LPN lpn, uint64_t ppn) {
        *(uint32_t *)(table + (lpn << 2)) = (uint32_t)ppn;
      };
    }
    else if (size < ((uint64_t)1ull << 48)) {
      uint64_t mask = (uint64_t)0x0000FFFFFFFFFFFF;
      ret = 6;

      readPointer = [this, mask](LPN lpn) {
        return ((uint64_t)(*(uint64_t *)(table + (lpn * 6)))) | mask;
      };
      writePointer = [this](LPN lpn, uint64_t ppn) {
        memcpy(table + lpn * 6, &ppn, 6);
      };
    }
    else {
      ret = 8;

      readPointer = [this](LPN lpn) {
        return (uint64_t)(*(uint64_t *)(table + (lpn << 3)));
      };
      writePointer = [this](LPN lpn, uint64_t ppn) {
        *(uint64_t *)(table + (lpn << 3)) = (uint64_t)ppn;
      };
    }

    return ret;
  }

  std::function<PPN(LPN)> readEntry;
  std::function<void(LPN, PPN)> writeEntry;
  std::function<uint64_t(LPN)> readPointer;
  std::function<void(LPN, uint64_t)> writePointer;

  void physicalSuperPageStats(uint64_t &, uint64_t &);
  CPU::Function readMappingInternal(LPN, PPN &);
  CPU::Function writeMappingInternal(LPN, bool, PPN &);
  CPU::Function invalidateMappingInternal(LPN, PPN &);

 public:
  VirtuallyLinked(ObjectData &, CommandManager *);
  ~VirtuallyLinked();

  void initialize(AbstractFTL *, BlockAllocator::AbstractAllocator *) override;

  LPN getPageUsage(LPN, LPN) override;

  //! SPPN -> SBLK
  virtual inline PPN getSBFromSPPN(PPN sppn) {
    return sppn % totalPhysicalSuperBlocks;
  }

  //! SPPN -> Page (Page index in (super)block)
  virtual inline PPN getPageIndexFromSPPN(PPN sppn) {
    return sppn / totalPhysicalSuperBlocks;
  }

  virtual inline LPN mappingGranularity() { return 1; }

  uint32_t getValidPages(PPN) override;
  uint16_t getAge(PPN) override;

  CPU::Function readMapping(Command &) override;
  CPU::Function writeMapping(Command &) override;
  CPU::Function invalidateMapping(Command &) override;
  void getCopyList(CopyList &, Event) override;
  void releaseCopyList(CopyList &) override;

  bool triggerMerge(bool);
  uint64_t getMergeReadCommand();
  uint64_t getMergeWriteCommand(uint64_t);
  void destroyMergeCommand(uint64_t);

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::FTL::Mapping

#endif
