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

#include "icl/cache/abstract_cache.hh"
#include "icl/manager/abstract_manager.hh"
#include "util/bitset.hh"

namespace SimpleSSD::ICL::Cache {

class SetAssociative : public AbstractCache {
 protected:
  uint32_t setSize;
  uint32_t waySize;

  uint32_t cacheTagSize;
  uint32_t cacheDataSize;
  uint64_t cacheTagBaseAddress;
  uint64_t cacheDataBaseAddress;

  uint64_t evictThreshold;
  uint64_t dirtyLines;

  std::vector<CacheLine> cacheline;
  std::function<CPU::Function(uint32_t, uint32_t &)> evictFunction;
  std::function<uint64_t(uint64_t, uint64_t)> compareFunction;

  struct LineInfo {
    uint32_t set;
    uint32_t way;

    LineInfo() {}
    LineInfo(uint32_t s, uint32_t w) : set(s), way(w) {}
  };

  // Lookup pending
  std::unordered_multimap<LPN, uint64_t> lookupList;

  // Pending (missed, but not allocated yet) list -- simillar to MSHR
  std::unordered_set<LPN> missList;
  std::unordered_multimap<LPN, uint64_t> missConflictList;

  // Flush
  struct FlushRequest {
    uint64_t tag;

    std::unordered_map<LPN, LineInfo> lpnList;

    FlushRequest() {}
    FlushRequest(uint64_t t) : tag(t) {}
  };

  std::list<FlushRequest> flushList;

  // Eviction
  std::unordered_map<LPN, LineInfo> evictList;

  // Allocation pending
  std::unordered_multimap<uint32_t, uint64_t> allocateList;

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

  void readAll(uint64_t, Event);
  void readSet(uint64_t, Event);
  void writeLine(uint64_t, uint32_t, uint32_t, Event);

  void tryLookup(LPN, bool = false);
  void tryAllocate(LPN);

  void collect(uint32_t, std::vector<Manager::FlushContext> &);

  Event eventLookupMemory;
  Event eventLookupDone;

  Event eventReadTag;

  Event eventCacheDone;

 public:
  SetAssociative(ObjectData &, Manager::AbstractManager *, FTL::Parameter *);
  ~SetAssociative();

  void lookup(HIL::SubRequest *) override;
  void flush(HIL::SubRequest *) override;
  void erase(HIL::SubRequest *) override;
  void allocate(HIL::SubRequest *) override;
  void dmaDone(LPN) override;
  void nvmDone(LPN, bool) override;

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::ICL::Cache

#endif
