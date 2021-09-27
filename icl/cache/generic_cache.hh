// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_ICL_CACHE_GENERIC_CACHE_HH__
#define __SIMPLESSD_ICL_CACHE_GENERIC_CACHE_HH__

#include <random>

#include "icl/cache/abstract_cache.hh"
#include "icl/cache/abstract_tagarray.hh"
#include "icl/manager/abstract_manager.hh"
#include "util/bitset.hh"

namespace SimpleSSD::ICL::Cache {

class GenericCache : public AbstractCache {
 protected:
  uint32_t cacheTagSize;
  uint32_t cacheDataSize;

  uint64_t evictThreshold;
  uint64_t dirtyLines;

  AbstractTagArray *tagArray;
  uint64_t totalTags;

  Log::DebugID logid;
  uint32_t superpage;

  // Lookup pending
  std::unordered_multimap<LPN, uint64_t> lookupList;

  // Pending (missed, but not allocated yet) list -- simillar to MSHR
  std::unordered_set<LPN> missList;
  std::unordered_multimap<LPN, uint64_t> missConflictList;

  // Allocation pending
  std::list<uint64_t> allocateList;

  // Writeback
  uint64_t pendingEviction;
  std::list<WritebackRequest> writebackList;

  void tryLookup(LPN);
  void tryAllocate(LPN);
  CPU::Function lookupImpl(HIL::SubRequest *sreq, bool &retry);

  inline void makeFlushContext(WritebackRequest &wbreq,
                               std::vector<Manager::FlushContext> &list) {
    list.reserve(wbreq.lpnList.size());

    for (auto &iter : wbreq.lpnList) {
      auto ctag = iter.second;
      auto &item = list.emplace_back(
          Manager::FlushContext(ctag->tag, tagArray->getDataAddress(ctag)));

      item.offset = ctag->validbits.ctz() * AbstractCache::minIO;
      item.length = cacheDataSize -
                    ctag->validbits.clz() * AbstractCache::minIO - item.offset;
    }
  }

  Event eventLookupDone;
  Event eventCacheDone;

 public:
  GenericCache(ObjectData &, Manager::AbstractManager *,
               const FTL::Parameter *);
  ~GenericCache();

  void lookup(HIL::SubRequest *) override;
  void flush(HIL::SubRequest *) override;
  void erase(HIL::SubRequest *) override;
  void allocate(HIL::SubRequest *) override;
  void dmaDone(HIL::SubRequest *) override;
  void drainDone(LPN, uint64_t) override;
  void nvmDone(LPN, uint64_t) override;

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::ICL::Cache

#endif
