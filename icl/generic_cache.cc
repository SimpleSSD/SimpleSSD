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

GenericCache::GenericCache(ConfigReader *c, FTL::FTL *f)
    : Cache(c, f), gen(rd()) {
  setSize = c->iclConfig.readUint(ICL_SET_SIZE);
  waySize = c->iclConfig.readUint(ICL_WAY_SIZE);
  lineSize = f->getInfo()->pageSize;

  dist = std::uniform_int_distribution<>(0, waySize - 1);

  useReadCaching = c->iclConfig.readBoolean(ICL_USE_READ_CACHE);
  useWriteCaching = c->iclConfig.readBoolean(ICL_USE_WRITE_CACHE);
  useReadPrefetch = c->iclConfig.readBoolean(ICL_USE_READ_PREFETCH);

  policy = (EVICT_POLICY)c->iclConfig.readInt(ICL_EVICT_POLICY);

  // TODO: replace this with DRAM model
  pTiming = c->iclConfig.getDRAMTiming();
  pStructure = c->iclConfig.getDRAMStructure();

  ppCache = (Line **)calloc(setSize, sizeof(Line *));

  for (uint32_t i = 0; i < setSize; i++) {
    ppCache[i] = new Line[waySize]();
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

uint32_t GenericCache::flushVictim(FTL::Request req, uint64_t &tick,
                                   bool *isCold) {
  uint32_t setIdx = calcSet(req.lpn);
  uint32_t entryIdx = waySize;
  uint64_t min = std::numeric_limits<uint64_t>::max();

  if (isCold) {
    *isCold = false;
  }

  // Check set has empty entry
  for (uint32_t i = 0; i < waySize; i++) {
    if (!ppCache[setIdx][i].valid) {
      entryIdx = i;

      if (isCold) {
        *isCold = true;
      }

      break;
    }
  }

  // If no empty entry
  if (entryIdx == waySize) {
    switch (policy) {
      case POLICY_RANDOM:
        entryIdx = dist(gen);

        break;
      case POLICY_FIFO:
        for (uint32_t i = 0; i < waySize; i++) {
          if (ppCache[setIdx][i].insertedAt < min) {
            min = ppCache[setIdx][i].insertedAt;
            entryIdx = i;
          }
        }

        break;
      case POLICY_LEAST_RECENTLY_USED:
        for (uint32_t i = 0; i < waySize; i++) {
          if (ppCache[setIdx][i].lastAccessed < min) {
            min = ppCache[setIdx][i].lastAccessed;
            entryIdx = i;
          }
        }

        break;
    }

    // Let's flush 'em if dirty
    if (ppCache[setIdx][entryIdx].dirty) {
      req.lpn = ppCache[setIdx][entryIdx].tag;
      req.offset = 0;
      req.length = lineSize;

      pFTL->write(req, tick);
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
  uint64_t pageCount =
      (bytesize > 0) ? (bytesize - 1) / pStructure->pageSize + 1 : 0;
  uint64_t pageFetch = pTiming->tRP + pTiming->tRCD + pTiming->tCL;
  double bandwidth =
      2.0 * pStructure->busWidth * pStructure->channel / 8.0 / pTiming->tCK;

  return (uint64_t)(pageFetch + pageCount * pStructure->pageSize / bandwidth);
}

// True when hit
bool GenericCache::read(FTL::Request &req, uint64_t &tick) {
  bool ret = false;

  Logger::debugprint(Logger::LOG_ICL_GENERIC_CACHE,
                     "READ  | LPN %" PRIu64 " | SIZE %" PRIu64, req.lpn,
                     req.length);

  if (useReadCaching) {
    uint32_t setIdx = calcSet(req.lpn);
    uint32_t entryIdx;
    uint64_t lat = calculateDelay(req.length);

    for (entryIdx = 0; entryIdx < waySize; entryIdx++) {
      Line &line = ppCache[setIdx][entryIdx];

      if (line.valid && line.tag == req.lpn) {
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
      entryIdx = flushVictim(req, tick, &cold);

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
      ppCache[setIdx][entryIdx].tag = req.lpn;
      ppCache[setIdx][entryIdx].lastAccessed = tick;
      ppCache[setIdx][entryIdx].insertedAt = tick;

      // Request read and flush at same time
      pFTL->read(req, tick);

      // DRAM delay should be hidden by NAND I/O
    }
  }
  else {
    pFTL->read(req, tick);
  }

  return ret;
}

// True when cold-miss/hit
bool GenericCache::write(FTL::Request &req, uint64_t &tick) {
  bool ret = false;

  Logger::debugprint(Logger::LOG_ICL_GENERIC_CACHE,
                     "WRITE | LPN %" PRIu64 " | SIZE %" PRIu64, req.lpn,
                     req.length);

  if (useWriteCaching) {
    uint32_t setIdx = calcSet(req.lpn);
    uint32_t entryIdx;
    uint64_t lat = calculateDelay(req.length);

    for (entryIdx = 0; entryIdx < waySize; entryIdx++) {
      Line &line = ppCache[setIdx][entryIdx];

      if (line.valid && line.tag == req.lpn) {
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
      entryIdx = flushVictim(req, tick, &cold);

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
      ppCache[setIdx][entryIdx].tag = req.lpn;
      ppCache[setIdx][entryIdx].lastAccessed = insertAt;
      ppCache[setIdx][entryIdx].insertedAt = insertAt;

      // DRAM delay should be hidden by NAND I/O
    }
  }
  else {
    pFTL->write(req, tick);
  }

  return ret;
}

// True when hit
bool GenericCache::flush(FTL::Request &req, uint64_t &tick) {
  bool ret = false;

  Logger::debugprint(Logger::LOG_ICL_GENERIC_CACHE, "FLUSH | LPN %" PRIu64,
                     req.lpn);

  if (useReadCaching || useWriteCaching) {
    uint32_t setIdx = calcSet(req.lpn);
    uint32_t i;

    for (i = 0; i < waySize; i++) {
      Line &line = ppCache[setIdx][i];

      if (line.valid && line.tag == req.lpn) {
        ret = true;

        break;
      }
    }

    if (ret) {
      if (ppCache[setIdx][i].dirty) {
        // we have to flush this
        pFTL->write(req, tick);
      }

      // invalidate
      ppCache[setIdx][i].valid = false;
    }
  }

  return ret;
}

// True when hit
bool GenericCache::trim(FTL::Request &req, uint64_t &tick) {
  bool ret = false;

  Logger::debugprint(Logger::LOG_ICL_GENERIC_CACHE, "TRIM  | LPN %" PRIu64,
                     req.lpn);

  if (useReadCaching || useWriteCaching) {
    uint32_t setIdx = calcSet(req.lpn);
    uint32_t i;

    for (i = 0; i < waySize; i++) {
      Line &line = ppCache[setIdx][i];

      if (line.valid && line.tag == req.lpn) {
        ret = true;

        break;
      }
    }

    if (ret) {
      // invalidate
      ppCache[setIdx][i].valid = false;
    }

    // we have to trim this
    pFTL->trim(req, tick);
  }
  else {
    pFTL->trim(req, tick);
  }

  return ret;
}

void GenericCache::format(LPNRange &range, uint64_t &tick) {
  bool ret = false;

  if (useReadCaching || useWriteCaching) {
    uint64_t lpn;
    uint32_t setIdx;
    uint32_t way;

    for (uint64_t i = 0; i < range.nlp; i++) {
      lpn = range.slpn + i;
      setIdx = calcSet(lpn);

      for (way = 0; way < waySize; way++) {
        Line &line = ppCache[setIdx][way];

        if (line.valid && line.tag == lpn) {
          ret = true;

          break;
        }
      }

      if (ret) {
        // invalidate
        ppCache[setIdx][way].valid = false;
      }
    }

    // we have to trim this
    pFTL->format(range, tick);
  }
  else {
    pFTL->format(range, tick);
  }
}

}  // namespace ICL

}  // namespace SimpleSSD
