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

#include "icl/generic_cache.hh"

#include <limits>

#include "util/algorithm.hh"

namespace SimpleSSD {

namespace ICL {

GenericCache::GenericCache(ConfigReader *c, FTL::FTL *f) : Cache(c, f) {
  setSize = c->iclConfig.readUint(ICL_SET_SIZE);
  entrySize = c->iclConfig.readUint(ICL_ENTRY_SIZE);
  lineSize = f->getInfo()->pageSize;

  useReadCaching = c->iclConfig.readBoolean(ICL_USE_READ_CACHE);
  useWriteCaching = c->iclConfig.readBoolean(ICL_USE_WRITE_CACHE);
  useReadPrefetch = c->iclConfig.readBoolean(ICL_USE_READ_PREFETCH);

  policy = (EVICT_POLICY)c->iclConfig.readInt(ICL_EVICT_POLICY);

  // TODO: replace this with DRAM model
  width = c->iclConfig.readUint(DRAM_CHIP_BUS_WIDTH);
  latency = c->iclConfig.readUint(DRAM_TIMING_RP);
  latency += c->iclConfig.readUint(DRAM_TIMING_RCD);
  latency += c->iclConfig.readUint(DRAM_TIMING_CL);

  latency *= lineSize / (width / 8);

  ppCache = (Line **)calloc(setSize, sizeof(Line *));

  for (uint32_t i = 0; i < setSize; i++) {
    ppCache[i] = new Line[entrySize]();
  }
}

GenericCache::~GenericCache() {
  for (uint32_t i = 0; i < setSize; i++) {
    delete[] ppCache[i];
  }

  free(ppCache);
}

uint32_t GenericCache::calcSet(uint64_t lpn) {
  return lpn & (setSize - 1);
}

uint32_t GenericCache::flushVictim(uint32_t setIdx, uint64_t &tick) {
  uint32_t entryIdx = entrySize;
  uint64_t min = std::numeric_limits<uint64_t>::max();

  // Check set has empty entry
  for (uint32_t i = 0; i < entrySize; i++) {
    if (!ppCache[setIdx][i].valid) {
      entryIdx = i;

      break;
    }
  }

  // If no empty entry
  if (entryIdx == entrySize) {
    switch (policy) {
      case POLICY_FIRST_ENTRY:
        entryIdx = 0;

        break;
      case POLICY_FIFO:
        for (uint32_t i = 0; i < entrySize; i++) {
          if (ppCache[setIdx][i].insertedAt < min) {
            min = ppCache[setIdx][i].insertedAt;
            entryIdx = i;
          }
        }

        break;
      case POLICY_LEAST_RECENTLY_USED:
        for (uint32_t i = 0; i < entrySize; i++) {
          if (ppCache[setIdx][i].lastAccessed < min) {
            min = ppCache[setIdx][i].lastAccessed;
            entryIdx = i;
          }
        }

        break;
    }

    // Let's flush 'em
    pFTL->write(ppCache[setIdx][entryIdx].tag, tick);

    // Invalidate
    ppCache[setIdx][entryIdx].valid = false;
  }

  return entryIdx;
}

// True when hit
bool GenericCache::read(uint64_t lpn, uint64_t &tick) {
  bool ret = false;

  if (useReadCaching) {
    uint32_t setIdx = calcSet(lpn);

    for (uint32_t i = 0; i < entrySize; i++) {
      Line &line = ppCache[setIdx][i];

      if (line.valid && line.tag == lpn) {
        line.lastAccessed = tick;
        ret = true;

        break;
      }
    }

    if (!ret) {
      uint64_t insertAt = tick;
      uint32_t emptyIdx = flushVictim(setIdx, tick);

      ppCache[setIdx][emptyIdx].valid = true;
      ppCache[setIdx][emptyIdx].dirty = false;
      ppCache[setIdx][emptyIdx].tag = lpn;
      ppCache[setIdx][emptyIdx].lastAccessed = insertAt;
      ppCache[setIdx][emptyIdx].insertedAt = insertAt;

      // Request read and flush at same time
      pFTL->read(lpn, insertAt);

      tick = MAX(tick, insertAt);
    }

    tick += latency;
  }
  else {
    pFTL->read(lpn, tick);
  }

  return ret;
}

// True when cold-miss/hit
bool GenericCache::write(uint64_t lpn, uint64_t &tick) {
  bool ret = false;

  if (useWriteCaching) {
    uint32_t setIdx = calcSet(lpn);

    for (uint32_t i = 0; i < entrySize; i++) {
      Line &line = ppCache[setIdx][i];

      if (line.valid && line.tag == lpn) {
        line.lastAccessed = tick;
        line.dirty = true;
        ret = true;

        break;
      }
    }

    if (!ret) {
      uint64_t insertAt = tick;
      uint32_t emptyIdx = flushVictim(setIdx, tick);

      ppCache[setIdx][emptyIdx].valid = true;
      ppCache[setIdx][emptyIdx].dirty = true;
      ppCache[setIdx][emptyIdx].tag = lpn;
      ppCache[setIdx][emptyIdx].lastAccessed = insertAt;
      ppCache[setIdx][emptyIdx].insertedAt = insertAt;
    }

    tick += latency;
  }
  else {
    pFTL->write(lpn, tick);
  }

  return ret;
}

// True when hit
bool GenericCache::flush(uint64_t lpn, uint64_t &tick) {
  bool ret = false;

  if (useReadCaching || useWriteCaching) {
    uint32_t setIdx = calcSet(lpn);
    uint32_t i;

    for (i = 0; i < entrySize; i++) {
      Line &line = ppCache[setIdx][i];

      if (line.valid && line.tag == lpn) {
        ret = true;

        break;
      }
    }

    if (ret) {
      if (ppCache[setIdx][i].dirty) {
        // we have to flush this
        pFTL->write(ppCache[setIdx][i].tag, tick);
      }

      // invalidate
      ppCache[setIdx][i].valid = false;
    }
  }

  return ret;
}

// True when hit
bool GenericCache::trim(uint64_t lpn, uint64_t &tick) {
  bool ret = false;

  if (useReadCaching || useWriteCaching) {
    uint32_t setIdx = calcSet(lpn);
    uint32_t i;

    for (i = 0; i < entrySize; i++) {
      Line &line = ppCache[setIdx][i];

      if (line.valid && line.tag == lpn) {
        ret = true;

        break;
      }
    }

    if (ret) {
      // invalidate
      ppCache[setIdx][i].valid = false;
    }

    // we have to trim this
    pFTL->trim(ppCache[setIdx][i].tag, tick);
  }
  else {
    pFTL->trim(lpn, tick);
  }

  return ret;
}

}  // namespace ICL

}  // namespace SimpleSSD
