// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_ICL_CACHE_RING_BUFFER_HH__
#define __SIMPLESSD_ICL_CACHE_RING_BUFFER_HH__

#include "icl/cache/abstract_tagarray.hh"
#include "icl/manager/abstract_manager.hh"

namespace SimpleSSD::ICL::Cache {

class RingBuffer : public AbstractTagArray {
 protected:
  uint64_t cacheTagBaseAddress;
  uint64_t cacheDataBaseAddress;

  uint64_t totalEntries;

  std::vector<CacheTag> cacheline;
  std::unordered_map<LPN, uint64_t> tagHashTable;

  std::function<CPU::Function(uint64_t &)> evictFunction;
  std::function<uint64_t(uint64_t, uint64_t)> compareFunction;

  // Victim selection functions
  CPU::Function fifoEviction(uint64_t &);
  CPU::Function lruEviction(uint64_t &);

  // Helper functions
  CPU::Function getValidLine(LPN, uint64_t &);
  CPU::Function getEmptyLine(uint64_t &);
  void getCleanLine(uint64_t &);

  inline uint64_t makeTagAddress(uint64_t idx) {
    return cacheTagBaseAddress + cacheTagSize * idx;
  }

  inline uint64_t makeDataAddress(uint64_t idx) {
    return cacheDataBaseAddress + cacheDataSize * idx;
  }

  Event eventReadTagAll;  // Read all tag
  void readAll(uint64_t, Event);

 public:
  RingBuffer(ObjectData &, Manager::AbstractManager *, const FTL::Parameter *);
  ~RingBuffer();

  uint64_t getArraySize() override;

  uint64_t getDataAddress(CacheTag *) override;

  // Event getLookupMemoryEvent() override;
  Event getReadAllMemoryEvent() override;
  // Event getWriteOneMemoryEvent() override;

  CPU::Function erase(LPN, uint32_t) override;

  bool checkAllocatable(LPN, HIL::SubRequest *) override;

  void collectEvictable(LPN, WritebackRequest &) override;
  void collectFlushable(LPN, uint32_t, WritebackRequest &) override;

  CPU::Function getValidLine(LPN, CacheTag **) override;
  CPU::Function getAllocatableLine(LPN, CacheTag **) override;

  std::string print(CacheTag *) noexcept override;
  Log::DebugID getLogID() noexcept override {
    return Log::DebugID::ICL_RingBuffer;
  }

  uint64_t getOffset(CacheTag *) const noexcept override;
  CacheTag *getTag(uint64_t) noexcept override;

  void getGCHint(FTL::GC::HintContext &) noexcept override;

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::ICL::Cache

#endif
