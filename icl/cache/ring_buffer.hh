// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_ICL_CACHE_RING_BUFFER_HH__
#define __SIMPLESSD_ICL_CACHE_RING_BUFFER_HH__

#include "icl/cache/abstract_cache.hh"

namespace SimpleSSD::ICL {

class RingBuffer : public AbstractCache {
 protected:
  uint32_t cacheTagSize;
  uint32_t cacheDataSize;
  uint64_t cacheTagBaseAddress;
  uint64_t cacheDataBaseAddress;

  uint64_t evictThreshold;
  uint64_t dirtyLines;

  uint64_t totalEntries;
  std::vector<CacheLine> cacheline;
  std::unordered_map<LPN, uint64_t> tagHashTable;

  std::function<CPU::Function(uint64_t &)> evictFunction;
  std::function<uint64_t(uint64_t, uint64_t)> compareFunction;

  // Lookup pending
  std::unordered_multimap<LPN, uint64_t> lookupList;

  // Miss pending
  std::unordered_set<LPN> missList;
  std::unordered_multimap<LPN, uint64_t> missConflictList;

  // Allocate pending
  std::list<uint64_t> allocateList;

  // Flush pending
  std::list<std::pair<uint64_t, std::unordered_map<LPN, uint64_t>>> flushList;

  // Evict pending
  std::unordered_map<LPN, uint64_t> evictList;

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

  void tryLookup(LPN, bool = false);
  void tryAllocate(LPN);

  void collect(std::vector<FlushContext> &);

  Event eventReadTagAll;  // Read all tag
  void readAll(uint64_t, Event);

  Event eventLookupDone;  // Call manager->lookupDone
  Event eventCacheDone;   // Call manager->cacheDone

 public:
  RingBuffer(ObjectData &, AbstractManager *, FTL::Parameter *);
  ~RingBuffer();

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

}  // namespace SimpleSSD::ICL

#endif
