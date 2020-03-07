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

  struct MemoryCommand {
    bool read;
    uint64_t address;
    uint32_t size;

    MemoryCommand(bool b, uint64_t a, uint32_t s)
        : read(b), address(a), size(s) {}
  };

  struct DemandPagingContext {
    uint64_t tag;
    LPN aligned;
    Event eid;
    uint64_t data;
    std::deque<MemoryCommand> cmdList;

    DemandPagingContext(uint64_t t, LPN l)
        : tag(t), aligned(l), eid(InvalidEventID), data(0) {}
  };

  std::deque<MemoryCommand> memoryCmdList;
  std::unordered_map<uint64_t, DemandPagingContext> memoryPending;

  uint64_t memoryTag;

  Event eventMemoryDone;
  void handleMemoryCommand(uint64_t);

  void insertMemoryAddress(bool, uint64_t, uint32_t, bool = true);
  inline uint64_t makeMemoryTag() { return ++memoryTag; }

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

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;
};

}  // namespace Mapping

}  // namespace SimpleSSD::FTL

#endif
