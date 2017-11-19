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

#include "log/trace.hh"
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
  pTiming = c->iclConfig.getDRAMTiming();
  pStructure = c->iclConfig.getDRAMStructure();

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

uint32_t GenericCache::flushVictim(uint32_t setIdx, uint64_t &tick,
                                   bool *isCold) {
  uint32_t entryIdx = entrySize;
  uint64_t min = std::numeric_limits<uint64_t>::max();

  if (isCold) {
    *isCold = false;
  }

  // Check set has empty entry
  for (uint32_t i = 0; i < entrySize; i++) {
    if (!ppCache[setIdx][i].valid) {
      entryIdx = i;

      if (isCold) {
        *isCold = true;
      }

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

    // Let's flush 'em if dirty
    if (ppCache[setIdx][entryIdx].dirty) {
      pFTL->write(ppCache[setIdx][entryIdx].tag, tick);
    }
    else {
      Logger::debugprint(Logger::LOG_ICL_GENERIC_CACHE,
                         "----- | Line (%u, %u) is clean, no need to flush",
                         setIdx, entryIdx);
    }

    // Invalidate
    ppCache[setIdx][entryIdx].valid = false;
  }

  return entryIdx;
}

uint64_t GenericCache::calculateDelay(uint64_t bytesize) {
  uint64_t pageCount = (bytesize > 0) ? (bytesize - 1) / pStructure->pageSize + 1 : 0;
  uint64_t pageFetch = pTiming->tRP + pTiming->tRCD + pTiming->tCL;
  double bandwidth = 2.0 * pStructure->busWidth * pStructure->channel / 8.0 / pTiming->tCK;

  return (uint64_t)(pageFetch + pageCount * pStructure->pageSize / bandwidth);
}

// True when hit
bool GenericCache::read(uint64_t lpn, uint64_t bytesize, uint64_t &tick) {
  bool ret = false;

  Logger::debugprint(Logger::LOG_ICL_GENERIC_CACHE,
                     "READ  | LPN %" PRIu64 " | SIZE %" PRIu64, lpn, bytesize);

  if (useReadCaching) {
    uint32_t setIdx = calcSet(lpn);
    uint32_t entryIdx;
    uint64_t lat = calculateDelay(bytesize);

    for (entryIdx = 0; entryIdx < entrySize; entryIdx++) {
      Line &line = ppCache[setIdx][entryIdx];

      if (line.valid && line.tag == lpn) {
        line.lastAccessed = tick;
        ret = true;

        break;
      }
    }

    if (ret) {
      Logger::debugprint(Logger::LOG_ICL_GENERIC_CACHE,
                         "READ  | Cache hit at (%u, %u) | %" PRIu64
                         " - %" PRIu64 " (%" PRIu64 ")",
                         setIdx, entryIdx, tick, tick + lat, lat);

      tick += lat;
    }
    else {
      Logger::debugprint(Logger::LOG_ICL_GENERIC_CACHE,
                         "READ  | Cache miss at %u", setIdx);

      bool cold;
      entryIdx = flushVictim(setIdx, tick, &cold);

      if (cold) {
        Logger::debugprint(Logger::LOG_ICL_GENERIC_CACHE,
                           "READ  | Cache cold-miss, no need to flush", setIdx);
      }
      else {
        Logger::debugprint(Logger::LOG_ICL_GENERIC_CACHE,
                           "READ  | Flush (%u, %u), LPN %" PRIu64, setIdx,
                           entryIdx, ppCache[setIdx][entryIdx].tag);
      }

      ppCache[setIdx][entryIdx].valid = true;
      ppCache[setIdx][entryIdx].dirty = false;
      ppCache[setIdx][entryIdx].tag = lpn;
      ppCache[setIdx][entryIdx].lastAccessed = tick;
      ppCache[setIdx][entryIdx].insertedAt = tick;

      // Request read and flush at same time
      pFTL->read(lpn, tick);

      // DRAM delay should be hidden by NAND I/O
    }
  }
  else {
    pFTL->read(lpn, tick);
  }

  return ret;
}

// True when cold-miss/hit
bool GenericCache::write(uint64_t lpn, uint64_t bytesize, uint64_t &tick) {
  bool ret = false;

  Logger::debugprint(Logger::LOG_ICL_GENERIC_CACHE,
                     "WRITE | LPN %" PRIu64 " | SIZE %" PRIu64, lpn, bytesize);

  if (useWriteCaching) {
    uint32_t setIdx = calcSet(lpn);
    uint32_t entryIdx;
    uint64_t lat = calculateDelay(bytesize);

    for (entryIdx = 0; entryIdx < entrySize; entryIdx++) {
      Line &line = ppCache[setIdx][entryIdx];

      if (line.valid && line.tag == lpn) {
        line.lastAccessed = tick;
        line.dirty = true;
        ret = true;

        break;
      }
    }

    if (ret) {
      Logger::debugprint(Logger::LOG_ICL_GENERIC_CACHE,
                         "WRITE | Cache hit at (%u, %u) | %" PRIu64
                         " - %" PRIu64 " (%" PRIu64 ")",
                         setIdx, entryIdx, tick, tick + lat, lat);

      tick += lat;
    }
    else {
      Logger::debugprint(Logger::LOG_ICL_GENERIC_CACHE,
                         "WRITE | Cache miss at %u", setIdx);

      bool cold;
      uint64_t insertAt = tick;
      entryIdx = flushVictim(setIdx, tick, &cold);

      if (cold) {
        Logger::debugprint(Logger::LOG_ICL_GENERIC_CACHE,
                           "WRITE | Cache cold-miss, no need to flush", setIdx);

        tick += lat;
      }
      else {
        Logger::debugprint(Logger::LOG_ICL_GENERIC_CACHE,
                           "WRITE | Flush (%u, %u), LPN %" PRIu64, setIdx,
                           entryIdx, ppCache[setIdx][entryIdx].tag);
      }

      ppCache[setIdx][entryIdx].valid = true;
      ppCache[setIdx][entryIdx].dirty = true;
      ppCache[setIdx][entryIdx].tag = lpn;
      ppCache[setIdx][entryIdx].lastAccessed = insertAt;
      ppCache[setIdx][entryIdx].insertedAt = insertAt;

      // DRAM delay should be hidden by NAND I/O
    }
  }
  else {
    pFTL->write(lpn, tick);
  }

  return ret;
}

// True when hit
bool GenericCache::flush(uint64_t lpn, uint64_t bytesize, uint64_t &tick) {
  bool ret = false;

  Logger::debugprint(Logger::LOG_ICL_GENERIC_CACHE, "FLUSH | LPN %" PRIu64,
                     lpn);

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
bool GenericCache::trim(uint64_t lpn, uint64_t bytesize, uint64_t &tick) {
  bool ret = false;

  Logger::debugprint(Logger::LOG_ICL_GENERIC_CACHE, "TRIM  | LPN %" PRIu64,
                     lpn);

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
