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

#include "icl/cache.hh"

namespace SimpleSSD {

namespace ICL {

class GenericCache : public Cache {
 private:
  uint32_t setSize;
  uint32_t waySize;
  uint32_t lineSize;

  bool useReadCaching;
  bool useWriteCaching;
  bool useReadPrefetch;

  EVICT_POLICY policy;
  std::random_device rd;
  std::mt19937 gen;
  std::uniform_int_distribution<> dist;

  // TODO: replace this with DRAM model
  Config::DRAMTiming *pTiming;
  Config::DRAMStructure *pStructure;

  Line **ppCache;

  uint32_t calcSet(uint64_t);
  uint32_t flushVictim(FTL::Request, uint64_t &, bool * = nullptr);
  uint64_t calculateDelay(uint64_t);

 public:
  GenericCache(ConfigReader *, FTL::FTL *);
  ~GenericCache() override;

  bool read(FTL::Request &, uint64_t &) override;
  bool write(FTL::Request &, uint64_t &) override;
  bool flush(FTL::Request &, uint64_t &) override;
  bool trim(FTL::Request &, uint64_t &) override;

  void format(LPNRange &, uint64_t &) override;
};

}  // namespace ICL

}  // namespace SimpleSSD

#endif
