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

#include <random>
#include <vector>

#include "icl/abstract_cache.hh"

namespace SimpleSSD {

namespace ICL {

class GenericCache : public AbstractCache {
 private:
  uint32_t setSize;
  uint32_t waySize;
  uint32_t lineSize;

  uint32_t partialIOUnitCount;
  uint32_t partialIOUnitSize;

  uint32_t prefetchIOCount;

  bool useReadCaching;
  bool useWriteCaching;
  bool useReadPrefetch;
  bool usePartialIO;

  Request lastRequest;
  bool prefetchEnabled;
  uint32_t hitCounter;

  EVICT_POLICY policy;
  std::random_device rd;
  std::mt19937 gen;
  std::uniform_int_distribution<> dist;

  // TODO: replace this with DRAM model
  Config::DRAMTiming *pTiming;
  Config::DRAMStructure *pStructure;

  std::vector<std::vector<Line>> ppCache;

  uint32_t calcSet(uint64_t);
  uint32_t flushVictim(Request, uint64_t &, bool * = nullptr);
  uint64_t calculateDelay(uint64_t);
  void convertIOFlag(DynamicBitset &, uint64_t, uint64_t);
  static void setBits(DynamicBitset &, uint64_t, uint64_t, bool);
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
