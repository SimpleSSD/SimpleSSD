/*
 * Copyright (C) 2017 CAMELab
 *
 * This file is part of SimpleSSD.
 *
 * SimpleSSD is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * SimpleSSD is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with SimpleSSD.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __ICL_GENERIC_CACHE__
#define __ICL_GENERIC_CACHE__

#include <functional>
#include <random>
#include <vector>

#include "icl/abstract_cache.hh"

namespace SimpleSSD {

namespace ICL {

struct EvictData {
  uint32_t setIdx;
  uint32_t wayIdx;
  uint64_t tag;

  EvictData();
};

class GenericCache : public AbstractCache {
 private:
  const uint32_t lineCountInSuperPage;
  const uint32_t superPageSize;
  const uint32_t waySize;
  const uint32_t lineSize;
  const uint32_t lineCountInMaxIO;
  uint32_t setSize;

  const uint32_t prefetchIOCount;
  const float prefetchIORatio;

  const bool useReadCaching;
  const bool useWriteCaching;
  const bool useReadPrefetch;

  Request lastRequest;
  bool prefetchEnabled;
  uint32_t hitCounter;
  uint32_t accessCounter;

  EVICT_POLICY policy;
  std::function<uint32_t(uint32_t)> evictFunction;
  std::random_device rd;
  std::mt19937 gen;
  std::uniform_int_distribution<> dist;

  // TODO: replace this with DRAM model
  Config::DRAMTiming *pTiming;
  Config::DRAMStructure *pStructure;

  std::vector<std::vector<Line>> ppCache;

  uint32_t calcSet(uint64_t);
  uint32_t getEmptyWay(uint32_t);
  uint32_t getValidWay(uint64_t);
  uint32_t getVictimWay(uint64_t);
  uint32_t getDirtyEntryCount(uint64_t, std::vector<EvictData> &);
  bool compareEvictList(std::vector<EvictData> &, std::vector<EvictData> &);
  uint64_t calculateDelay(uint64_t);
  void evictVictim(std::vector<EvictData> &, bool, uint64_t &);
  void checkPrefetch(Request &);

 public:
  GenericCache(ConfigReader *, FTL::FTL *);
  ~GenericCache();

  bool read(Request &, uint64_t &) override;
  bool write(Request &, uint64_t &) override;
  bool flush(Request &, uint64_t &) override;
  bool trim(Request &, uint64_t &) override;

  void format(LPNRange &, uint64_t &) override;
};

}  // namespace ICL

}  // namespace SimpleSSD

#endif
