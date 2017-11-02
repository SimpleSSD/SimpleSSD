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

#include <unordered_map>

#include "icl/cache.hh"

namespace SimpleSSD {

namespace ICL {

class GenericCache : public Cache {
 private:
  uint32_t setSize;
  uint32_t entrySize;
  uint32_t lineSize;

  bool useReadCaching;
  bool useWriteCaching;
  bool useReadPrefetch;

  EVICT_POLICY policy;

  // TODO: replace this with DRAM model
  uint64_t latency;
  uint32_t width;

  Line **ppCache;

  uint32_t calcSet(uint64_t);
  uint32_t flushVictim(uint32_t, uint64_t &);

 public:
  GenericCache(ConfigReader *, FTL::FTL *);
  ~GenericCache();

  bool read(uint64_t, uint64_t &);
  bool write(uint64_t, uint64_t &);
  bool flush(uint64_t, uint64_t &);
  bool trim(uint64_t, uint64_t &);
};

}  // namespace ICL

}  // namespace SimpleSSD

#endif
