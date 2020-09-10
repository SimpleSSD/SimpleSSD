// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_ICL_CACHE_SET_ASSOCIATIVE_HH__
#define __SIMPLESSD_ICL_CACHE_SET_ASSOCIATIVE_HH__

#include <random>

#include "icl/cache/abstract_tagarray.hh"
#include "icl/manager/abstract_manager.hh"
#include "util/bitset.hh"

namespace SimpleSSD::ICL::Cache {

class SetAssociative : public AbstractTagArray {
 protected:
  uint32_t setSize;
  uint32_t waySize;

  uint64_t cacheTagSize;
  uint64_t cacheDataSize;
  uint64_t cacheTagBaseAddress;
  uint64_t cacheDataBaseAddress;

  std::vector<CacheTag> cacheline;
  std::function<CPU::Function(uint32_t, uint32_t &)> evictFunction;
  std::function<uint64_t(uint64_t, uint64_t)> compareFunction;

  struct LineInfo {
    uint32_t set;
    uint32_t way;

    LineInfo() {}
    LineInfo(uint32_t s, uint32_t w) : set(s), way(w) {}
  };

  // Victim selection
  CPU::Function fifoEviction(uint32_t, uint32_t &);
  CPU::Function lruEviction(uint32_t, uint32_t &);

  inline uint32_t getSetIdx(LPN addr) { return addr % setSize; }
  CPU::Function getEmptyWay(uint32_t, uint32_t &);
  CPU::Function getValidWay(LPN, uint32_t &);
  void getCleanWay(uint32_t, uint32_t &);

  inline uint64_t makeTagAddress(uint32_t set) {
    return cacheTagBaseAddress + cacheTagSize * waySize * set;
  }

  inline uint64_t makeTagAddress(uint32_t set, uint32_t way) {
    return cacheTagBaseAddress + cacheTagSize * (waySize * set + way);
  }

  inline uint64_t makeDataAddress(uint32_t set) {
    return cacheDataBaseAddress + cacheDataSize * waySize * set;
  }

  inline uint64_t makeDataAddress(uint32_t set, uint32_t way) {
    return cacheDataBaseAddress + cacheDataSize * (waySize * set + way);
  }

  inline void tagToLine(CacheTag *ctag, uint32_t &set, uint32_t &way) {
    uint64_t offset = getOffset(ctag);

    set = offset / waySize;
    way = offset % waySize;
  }

  inline CacheTag *lineToTag(uint32_t set, uint32_t way) {
    return cacheline.data() + set * waySize + way;
  }

  void readAll(uint64_t, Event);
  void readSet(uint64_t, Event);
  void writeLine(uint64_t, uint32_t, uint32_t, Event);

  Event eventReadAll;
  Event eventReadSet;
  Event eventWriteOne;

 public:
  SetAssociative(ObjectData &, Manager::AbstractManager *,
                 const FTL::Parameter *);
  ~SetAssociative();

  uint64_t getArraySize() override;

  uint64_t getDataAddress(CacheTag *) override;

  Event getLookupMemoryEvent() override;
  Event getReadAllMemoryEvent() override;
  Event getWriteOneMemoryEvent() override;

  CPU::Function erase(LPN, uint32_t) override;

  bool checkAllocatable(LPN, HIL::SubRequest *) override;

  void collectEvictable(LPN, WritebackRequest &) override;
  void collectFlushable(LPN, uint32_t, WritebackRequest &) override;

  CPU::Function getValidLine(LPN, CacheTag **) override;
  CPU::Function getAllocatableLine(LPN, CacheTag **) override;

  std::string print(CacheTag *) noexcept override;
  Log::DebugID getLogID() noexcept override {
    return Log::DebugID::ICL_SetAssociative;
  }

  uint64_t getOffset(CacheTag *) const noexcept override;
  CacheTag *getTag(uint64_t) noexcept override;

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::ICL::Cache

#endif
