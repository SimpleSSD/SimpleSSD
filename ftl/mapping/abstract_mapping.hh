// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_FTL_MAPPING_ABSTRACT_MAPPING_HH__
#define __SIMPLESSD_FTL_MAPPING_ABSTRACT_MAPPING_HH__

#include "ftl/def.hh"
#include "hil/request.hh"
#include "sim/object.hh"

namespace SimpleSSD::FTL {

class AbstractFTL;

namespace BlockAllocator {

class AbstractAllocator;

}

namespace Mapping {

struct BlockMetadata {
  PPN blockID;

  uint32_t nextPageToWrite;
  uint64_t insertedAt;
  Bitset validPages;

  BlockMetadata() : blockID(InvalidPPN), nextPageToWrite(0), insertedAt(0) {}
  BlockMetadata(PPN id, uint32_t s)
      : blockID(id), nextPageToWrite(0), insertedAt(0), validPages(s) {}
};

class AbstractMapping : public Object {
 protected:
  using ReadEntryFunction = std::function<uint64_t(uint8_t *, uint64_t)>;
  using WriteEntryFunction = std::function<void(uint8_t *, uint64_t, uint64_t)>;
  using ParseEntryFunction = std::function<uint64_t(uint64_t &)>;
  using MakeEntryFunction = std::function<uint64_t(uint64_t, uint64_t)>;

  Parameter param;
  FIL::Config::NANDStructure *filparam;

  AbstractFTL *pFTL;
  BlockAllocator::AbstractAllocator *allocator;

  // Stat
  uint64_t requestedReadCount;
  uint64_t requestedWriteCount;
  uint64_t requestedInvalidateCount;
  uint64_t readLPNCount;
  uint64_t writeLPNCount;
  uint64_t invalidateLPNCount;

  virtual void makeSpare(LPN, std::vector<uint8_t> &);
  virtual LPN readSpare(std::vector<uint8_t> &);

  inline uint64_t makeEntrySize(uint64_t total, uint64_t shift,
                                ReadEntryFunction &readFunc,
                                WriteEntryFunction &writeFunc,
                                ParseEntryFunction &parseMetaFunc,
                                MakeEntryFunction &makeMetaFunc) {
    uint64_t ret = 8;

    total <<= shift;

    // Memory consumption optimization
    if (total <= std::numeric_limits<uint16_t>::max()) {
      ret = 2;

      readFunc = [](uint8_t *table, uint64_t offset) {
        return *(uint16_t *)(table + (offset << 1));
      };
      writeFunc = [](uint8_t *table, uint64_t offset, uint64_t value) {
        *(uint16_t *)(table + (offset << 1)) = (uint16_t)value;
      };
    }
    else if (total <= std::numeric_limits<uint32_t>::max()) {
      ret = 4;

      readFunc = [](uint8_t *table, uint64_t offset) {
        return *(uint32_t *)(table + (offset << 2));
      };
      writeFunc = [](uint8_t *table, uint64_t offset, uint64_t value) {
        *(uint32_t *)(table + (offset << 2)) = (uint32_t)value;
      };
    }
    else if (total <= ((uint64_t)1ull << 48)) {
      uint64_t mask = (uint64_t)0x0000FFFFFFFFFFFF;
      ret = 6;

      readFunc = [mask](uint8_t *table, uint64_t offset) {
        return (*(uint64_t *)(table + (offset * 6))) & mask;
      };
      writeFunc = [](uint8_t *table, uint64_t offset, uint64_t value) {
        memcpy(table + offset * 6, &value, 6);
      };
    }
    else {
      ret = 8;

      readFunc = [](uint8_t *table, uint64_t offset) {
        return *(uint64_t *)(table + (offset << 3));
      };
      writeFunc = [](uint8_t *table, uint64_t offset, uint64_t value) {
        *(uint64_t *)(table + (offset << 3)) = value;
      };
    }

    shift = ret * 8 - shift;
    uint64_t mask = ((uint64_t)1 << shift) - 1;

    parseMetaFunc = [shift, mask](uint64_t &entry) {
      uint64_t ret = entry >> shift;

      entry &= mask;

      return ret;
    };
    makeMetaFunc = [shift, mask](uint64_t entry, uint64_t meta) {
      return (entry & mask) | (meta << shift);
    };

    return ret;
  }

  void insertMemoryAddress(bool, uint64_t, uint32_t, bool = true);
  void requestMemoryAccess(Event, uint64_t, CPU::Function &);

 private:
  struct MemoryCommand {
    uint64_t address;
    bool read;
    uint32_t size;

    MemoryCommand(bool b, uint64_t a, uint32_t s)
        : address(a), read(b), size(s) {}
  };

  struct CommandList {
    // Request information
    Event eid;
    uint64_t data;

    // Firmware information
    CPU::Function fstat;

    // Pending memory access list
    std::deque<MemoryCommand> cmdList;
  };

  std::deque<MemoryCommand> pendingMemoryAccess;
  std::unordered_map<uint64_t, CommandList> memoryCommandList;

  uint64_t memoryTag;
  inline uint64_t makeMemoryTag() { return ++memoryTag; }

  Event eventMemoryDone;
  void handleMemoryCommand(uint64_t);

 public:
  AbstractMapping(ObjectData &);
  virtual ~AbstractMapping() {}

  virtual void initialize(AbstractFTL *, BlockAllocator::AbstractAllocator *);

  Parameter *getInfo();
  virtual LPN getPageUsage(LPN, LPN) = 0;

  // Allocator
  virtual uint32_t getValidPages(PPN) = 0;
  virtual uint64_t getAge(PPN) = 0;

  // I/O interfaces
  virtual void readMapping(Request *, Event) = 0;
  virtual void writeMapping(Request *, Event) = 0;
  virtual void invalidateMapping(Request *, Event) = 0;

  /**
   * \brief Get minimum and preferred mapping granularity
   *
   * Return minimum (which not making read-modify-write operation) mapping
   * granularity and preferred (which can make best performance) mapping size.
   *
   * \param[out]  min     Minimum mapping granularity
   * \param[out]  prefer  Preferred mapping granularity
   */
  virtual void getMappingSize(uint64_t *min, uint64_t *prefer = nullptr) = 0;

  // GC interfaces
  virtual void getCopyList(CopyList &, Event) = 0;
  virtual void releaseCopyList(CopyList &) = 0;

  //! PPN -> Blk
  inline PPN getBlockFromPPN(PPN ppn) {
    return ppn % param.totalPhysicalBlocks;
  }

  //! PPN -> Page
  inline PPN getPageFromPPN(PPN ppn) { return ppn / param.totalPhysicalBlocks; }

  //! Blk/Page -> PPN
  inline PPN makePPN(PPN block, PPN page) {
    return block + page * param.totalPhysicalBlocks;
  }

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace Mapping

}  // namespace SimpleSSD::FTL

#endif
