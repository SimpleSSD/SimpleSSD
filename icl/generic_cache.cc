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

#include <algorithm>
#include <cstddef>
#include <limits>

#include "util/algorithm.hh"

namespace SimpleSSD {

namespace ICL {

#define CACHE_DELAY 20

GenericCache::GenericCache(ConfigReader &c, FTL::FTL *f, DRAM::AbstractDRAM *d)
    : AbstractCache(c, f, d),
      lineCountInSuperPage(f->getInfo()->ioUnitInPage),
      superPageSize(f->getInfo()->pageSize),
      lineSize(superPageSize / lineCountInSuperPage),
      parallelIO(f->getInfo()->pageCountToMaxPerf),
      lineCountInMaxIO(parallelIO * lineCountInSuperPage),
      waySize(conf.readUint(CONFIG_ICL, ICL_WAY_SIZE)),
      prefetchIOCount(conf.readUint(CONFIG_ICL, ICL_PREFETCH_COUNT)),
      prefetchIORatio(conf.readFloat(CONFIG_ICL, ICL_PREFETCH_RATIO)),
      useReadCaching(conf.readBoolean(CONFIG_ICL, ICL_USE_READ_CACHE)),
      useWriteCaching(conf.readBoolean(CONFIG_ICL, ICL_USE_WRITE_CACHE)),
      useReadPrefetch(conf.readBoolean(CONFIG_ICL, ICL_USE_READ_PREFETCH)),
      gen(rd()),
      dist(std::uniform_int_distribution<>(0, waySize - 1)) {
  uint64_t cacheSize = conf.readUint(CONFIG_ICL, ICL_CACHE_SIZE);

  if (!useReadCaching && !useWriteCaching) {
    return;
  }

  // Fully-associated?
  if (waySize == 0) {
    setSize = 1;
    waySize = MAX(cacheSize / lineSize, 1);
  }
  else {
    setSize = MAX(cacheSize / lineSize / waySize, 1);
  }

  debugprint(
      LOG_ICL_GENERIC_CACHE,
      "CREATE  | Set size %u | Way size %u | Line size %u | Capacity %" PRIu64,
      setSize, waySize, lineSize, (uint64_t)setSize * waySize * lineSize);
  debugprint(LOG_ICL_GENERIC_CACHE,
             "CREATE  | line count in super page %u | line count in max I/O %u",
             lineCountInSuperPage, lineCountInMaxIO);

  cacheData.resize(setSize);

  for (uint32_t i = 0; i < setSize; i++) {
    cacheData[i] = new Line[waySize]();
  }

  evictData.resize(lineCountInSuperPage);

  for (uint32_t i = 0; i < lineCountInSuperPage; i++) {
    evictData[i] = (Line **)calloc(parallelIO, sizeof(Line *));
  }

  lastRequest.reqID = 1;
  prefetchEnabled = false;
  hitCounter = 0;
  accessCounter = 0;

  // Set evict policy functional
  policy = (EVICT_POLICY)conf.readInt(CONFIG_ICL, ICL_EVICT_POLICY);

  switch (policy) {
    case POLICY_RANDOM:
      evictFunction = [this](uint32_t setIdx, uint64_t &tick) -> uint32_t {
        return dist(gen);
      };
      compareFunction = [this](Line *a, Line *b) -> Line * {
        if (a && b) {
          return dist(gen) > waySize / 2 ? a : b;
        }
        else if (a || b) {
          return a ? a : b;
        }
        else {
          return nullptr;
        }
      };

      break;
    case POLICY_FIFO:
      evictFunction = [this](uint32_t setIdx, uint64_t &tick) -> uint32_t {
        uint32_t wayIdx = 0;
        uint64_t min = std::numeric_limits<uint64_t>::max();

        for (uint32_t i = 0; i < waySize; i++) {
          tick += CACHE_DELAY * 8;
          // pDRAM->read(MAKE_META_ADDR(setIdx, i, offsetof(Line, insertedAt)),
          // 8, tick);

          if (cacheData[setIdx][i].insertedAt < min) {
            min = cacheData[setIdx][i].insertedAt;
            wayIdx = i;
          }
        }

        return wayIdx;
      };
      compareFunction = [](Line *a, Line *b) -> Line * {
        if (a && b) {
          if (a->insertedAt < b->insertedAt) {
            return a;
          }
          else {
            return b;
          }
        }
        else if (a || b) {
          return a ? a : b;
        }
        else {
          return nullptr;
        }
      };

      break;
    case POLICY_LEAST_RECENTLY_USED:
      evictFunction = [this](uint32_t setIdx, uint64_t &tick) -> uint32_t {
        uint32_t wayIdx = 0;
        uint64_t min = std::numeric_limits<uint64_t>::max();

        for (uint32_t i = 0; i < waySize; i++) {
          tick += CACHE_DELAY * 8;
          // pDRAM->read(MAKE_META_ADDR(setIdx, i, offsetof(Line,
          // lastAccessed)), 8, tick);

          if (cacheData[setIdx][i].lastAccessed < min) {
            min = cacheData[setIdx][i].lastAccessed;
            wayIdx = i;
          }
        }

        return wayIdx;
      };
      compareFunction = [](Line *a, Line *b) -> Line * {
        if (a && b) {
          if (a->lastAccessed < b->lastAccessed) {
            return a;
          }
          else {
            return b;
          }
        }
        else if (a || b) {
          return a ? a : b;
        }
        else {
          return nullptr;
        }
      };

      break;
    default:
      panic("Undefined cache evict policy");

      break;
  }

  memset(&stat, 0, sizeof(stat));
}

GenericCache::~GenericCache() {
  for (uint32_t i = 0; i < setSize; i++) {
    delete[] cacheData[i];
  }

  for (uint32_t i = 0; i < lineCountInSuperPage; i++) {
    free(evictData[i]);
  }
}

uint32_t GenericCache::calcSetIndex(uint64_t lca) {
  return lca % setSize;
}

void GenericCache::calcIOPosition(uint64_t lca, uint32_t &row, uint32_t &col) {
  uint32_t tmp = lca % lineCountInMaxIO;

  row = tmp % lineCountInSuperPage;
  col = tmp / lineCountInSuperPage;
}

uint32_t GenericCache::getEmptyWay(uint32_t setIdx, uint64_t &tick) {
  uint32_t retIdx = waySize;
  uint64_t minInsertedAt = std::numeric_limits<uint64_t>::max();

  for (uint32_t wayIdx = 0; wayIdx < waySize; wayIdx++) {
    Line &line = cacheData[setIdx][wayIdx];

    if (!line.valid) {
      tick += CACHE_DELAY * 8;
      // pDRAM->read(MAKE_META_ADDR(setIdx, wayIdx, offsetof(Line, insertedAt)),
      // 8, tick);

      if (minInsertedAt > line.insertedAt) {
        minInsertedAt = line.insertedAt;
        retIdx = wayIdx;
      }
    }
  }

  return retIdx;
}

uint32_t GenericCache::getValidWay(uint64_t lca, uint64_t &tick) {
  uint32_t setIdx = calcSetIndex(lca);
  uint32_t wayIdx;

  for (wayIdx = 0; wayIdx < waySize; wayIdx++) {
    Line &line = cacheData[setIdx][wayIdx];

    tick += CACHE_DELAY * 8;
    // pDRAM->read(MAKE_META_ADDR(setIdx, wayIdx, offsetof(Line, tag)), 8,
    // tick);

    if (line.valid && line.tag == lca) {
      break;
    }
  }

  return wayIdx;
}

void GenericCache::checkPrefetch(Request &req) {
  if (lastRequest.reqID == req.reqID) {
    lastRequest.range = req.range;
    lastRequest.offset = req.offset;
    lastRequest.length = req.length;

    return;
  }

  if (lastRequest.range.slpn * lineSize + lastRequest.offset +
          lastRequest.length ==
      req.range.slpn * lineSize + req.offset) {
    if (!prefetchEnabled) {
      hitCounter++;
      accessCounter += req.length;

      if (hitCounter >= prefetchIOCount &&
          (float)accessCounter / superPageSize >= prefetchIORatio) {
        prefetchEnabled = true;
      }
    }
  }
  else {
    prefetchEnabled = false;
    hitCounter = 0;
    accessCounter = 0;
  }

  lastRequest = req;
}

void GenericCache::evictCache(uint64_t tick) {
  FTL::Request reqInternal(lineCountInSuperPage);
  uint64_t beginAt;
  uint64_t finishedAt = tick;

  debugprint(LOG_ICL_GENERIC_CACHE, "----- | Begin eviction");

  for (uint32_t row = 0; row < lineCountInSuperPage; row++) {
    for (uint32_t col = 0; col < parallelIO; col++) {
      beginAt = tick;

      if (evictData[row][col] == nullptr) {
        continue;
      }

      if (evictData[row][col]->valid && evictData[row][col]->dirty) {
        reqInternal.lpn = evictData[row][col]->tag / lineCountInSuperPage;
        reqInternal.ioFlag.reset();
        reqInternal.ioFlag.set(row);

        pFTL->write(reqInternal, beginAt);
      }

      evictData[row][col]->insertedAt = beginAt;
      evictData[row][col]->lastAccessed = beginAt;
      evictData[row][col]->valid = false;
      evictData[row][col]->dirty = false;
      evictData[row][col]->tag = 0;
      evictData[row][col] = nullptr;

      finishedAt = MAX(finishedAt, beginAt);
    }
  }

  debugprint(LOG_ICL_GENERIC_CACHE,
             "----- | End eviction | %" PRIu64 " - %" PRIu64 " (%" PRIu64 ")",
             tick, finishedAt, finishedAt - tick);
}

// True when hit
bool GenericCache::read(Request &req, uint64_t &tick) {
  bool ret = false;

  debugprint(LOG_ICL_GENERIC_CACHE,
             "READ  | REQ %7u-%-4u | LCA %" PRIu64 " | SIZE %" PRIu64,
             req.reqID, req.reqSubID, req.range.slpn, req.length);

  if (useReadCaching) {
    uint32_t setIdx = calcSetIndex(req.range.slpn);
    uint32_t wayIdx;

    if (useReadPrefetch) {
      checkPrefetch(req);
    }

    wayIdx = getValidWay(req.range.slpn, tick);

    // Do we have valid data?
    if (wayIdx != waySize) {
      uint64_t arrived = tick;

      // Wait cache to be valid
      if (tick < cacheData[setIdx][wayIdx].insertedAt) {
        tick = cacheData[setIdx][wayIdx].insertedAt;
      }

      // Update last accessed time
      cacheData[setIdx][wayIdx].lastAccessed = tick;

      // DRAM access
      pDRAM->read(&cacheData[setIdx][wayIdx], req.length, tick);

      debugprint(LOG_ICL_GENERIC_CACHE,
                 "READ  | Cache hit at (%u, %u) | %" PRIu64 " - %" PRIu64
                 " (%" PRIu64 ")",
                 setIdx, wayIdx, arrived, tick, tick - arrived);

      ret = true;
    }
    // We should read data from NVM
    else {
      FTL::Request reqInternal(lineCountInSuperPage, req);
      std::vector<std::pair<uint64_t, uint64_t>> readList;
      uint32_t row, col;  // Variable for I/O position (IOFlag)
      uint64_t dramAt;
      uint64_t beginLCA, endLCA;
      uint64_t beginAt, finishedAt = tick;

      if (prefetchEnabled) {
        beginLCA = req.range.slpn;
        endLCA = beginLCA + lineCountInMaxIO;
      }
      else {
        beginLCA = req.range.slpn;
        endLCA = beginLCA + 1;
      }

      for (uint64_t lca = beginLCA; lca < endLCA; lca++) {
        // Check cache
        if (getValidWay(lca, beginAt) != waySize) {
          continue;
        }

        // Find way to write data read from NVM
        setIdx = calcSetIndex(lca);
        wayIdx = getEmptyWay(setIdx, tick);

        if (wayIdx == waySize) {
          wayIdx = evictFunction(setIdx, tick);

          if (cacheData[setIdx][wayIdx].dirty) {
            // We need to evict data before write
            calcIOPosition(cacheData[setIdx][wayIdx].tag, row, col);
            evictData[row][col] = cacheData[setIdx] + wayIdx;
          }
        }

        cacheData[setIdx][wayIdx].insertedAt = tick;
        cacheData[setIdx][wayIdx].lastAccessed = tick;
        cacheData[setIdx][wayIdx].valid = true;
        cacheData[setIdx][wayIdx].dirty = false;

        readList.push_back({lca, ((uint64_t)setIdx << 32) | wayIdx});
      }

      evictCache(tick);

      for (auto &iter : readList) {
        Line *pLine = &cacheData[iter.second >> 32][iter.second & 0xFFFFFFFF];

        // Read data
        reqInternal.lpn = iter.first / lineCountInSuperPage;
        reqInternal.ioFlag.reset();
        reqInternal.ioFlag.set(iter.first % lineCountInSuperPage);

        beginAt = tick;  // Ignore cache metadata access
        pFTL->read(reqInternal, beginAt);

        // DRAM delay
        dramAt = pLine->insertedAt;
        pDRAM->read(pLine, lineSize, dramAt);

        // Set cache data
        beginAt = MAX(beginAt, dramAt);

        pLine->insertedAt = beginAt;
        pLine->lastAccessed = beginAt;
        pLine->tag = iter.first;

        if (pLine->tag == req.range.slpn) {
          finishedAt = beginAt;
        }

        debugprint(LOG_ICL_GENERIC_CACHE,
                   "READ  | Cache miss at (%u, %u) | %" PRIu64 " - %" PRIu64
                   " (%" PRIu64 ")",
                   iter.second >> 32, iter.second & 0xFFFFFFFF, tick, beginAt,
                   beginAt - tick);
      }

      tick = finishedAt;
    }
  }
  else {
    FTL::Request reqInternal(lineCountInSuperPage, req);

    pFTL->read(reqInternal, tick);
  }

  stat.request[0]++;

  if (ret) {
    stat.cache[0]++;
  }

  return ret;
}

// True when cold-miss/hit
bool GenericCache::write(Request &req, uint64_t &tick) {
  bool ret = false;

  debugprint(LOG_ICL_GENERIC_CACHE,
             "WRITE | REQ %7u-%-4u | LCA %" PRIu64 " | SIZE %" PRIu64,
             req.reqID, req.reqSubID, req.range.slpn, req.length);

  if (useWriteCaching) {
    uint32_t setIdx = calcSetIndex(req.range.slpn);
    uint32_t wayIdx;

    wayIdx = getValidWay(req.range.slpn, tick);

    // Can we update old data?
    if (wayIdx != waySize) {
      uint64_t arrived = tick;

      // Wait cache to be valid
      if (tick < cacheData[setIdx][wayIdx].insertedAt) {
        tick = cacheData[setIdx][wayIdx].insertedAt;
      }

      // Update last accessed time
      cacheData[setIdx][wayIdx].insertedAt = tick;
      cacheData[setIdx][wayIdx].lastAccessed = tick;
      cacheData[setIdx][wayIdx].dirty = true;

      // DRAM access
      pDRAM->read(&cacheData[setIdx][wayIdx], req.length, tick);

      debugprint(LOG_ICL_GENERIC_CACHE,
                 "WRITE | Cache hit at (%u, %u) | %" PRIu64 " - %" PRIu64
                 " (%" PRIu64 ")",
                 setIdx, wayIdx, arrived, tick, tick - arrived);

      ret = true;
    }
    else {
      uint64_t arrived = tick;

      wayIdx = getEmptyWay(setIdx, tick);

      // Do we have place to write data?
      if (wayIdx != waySize) {
        // Wait cache to be valid
        if (tick < cacheData[setIdx][wayIdx].insertedAt) {
          tick = cacheData[setIdx][wayIdx].insertedAt;
        }

        // Update last accessed time
        cacheData[setIdx][wayIdx].insertedAt = tick;
        cacheData[setIdx][wayIdx].lastAccessed = tick;
        cacheData[setIdx][wayIdx].valid = true;
        cacheData[setIdx][wayIdx].dirty = true;
        cacheData[setIdx][wayIdx].tag = req.range.slpn;

        // DRAM access
        pDRAM->read(&cacheData[setIdx][wayIdx], req.length, tick);

        ret = true;
      }
      // We have to flush
      else {
        uint32_t row, col;  // Variable for I/O position (IOFlag)

        for (setIdx = 0; setIdx < setSize; setIdx++) {
          for (wayIdx = 0; wayIdx < waySize; wayIdx++) {
            if (cacheData[setIdx][wayIdx].valid &&
                cacheData[setIdx][wayIdx].dirty) {
              calcIOPosition(cacheData[setIdx][wayIdx].tag, row, col);

              evictData[row][col] = compareFunction(evictData[row][col],
                                                    cacheData[setIdx] + wayIdx);
            }
          }
        }

        tick += CACHE_DELAY * setSize * waySize * 8;

        evictCache(tick);

        // Update cacheline of current request
        setIdx = calcSetIndex(req.range.slpn);
        wayIdx = getEmptyWay(setIdx, tick);

        if (wayIdx == waySize) {
          panic("Cache corrupted!");
        }

        // DRAM latency
        pDRAM->write(&cacheData[setIdx][wayIdx], req.length, tick);

        // Update cache data
        cacheData[setIdx][wayIdx].insertedAt = tick;
        cacheData[setIdx][wayIdx].lastAccessed = tick;
        cacheData[setIdx][wayIdx].valid = true;
        cacheData[setIdx][wayIdx].dirty = true;
        cacheData[setIdx][wayIdx].tag = req.range.slpn;
      }

      debugprint(LOG_ICL_GENERIC_CACHE,
                 "WRITE | Cache miss at (%u, %u) | %" PRIu64 " - %" PRIu64
                 " (%" PRIu64 ")",
                 setIdx, wayIdx, arrived, tick, tick - arrived);
    }
  }
  else {
    FTL::Request reqInternal(lineCountInSuperPage, req);

    pFTL->write(reqInternal, tick);
  }

  stat.request[1]++;

  if (ret) {
    stat.cache[1]++;
  }

  return ret;
}

// True when flushed
bool GenericCache::flush(Request &req, uint64_t &tick) {
  bool ret = false;

  if (useReadCaching || useWriteCaching) {
    uint32_t setIdx = calcSetIndex(req.range.slpn);
    uint32_t wayIdx;

    // Check cache that we have data for corresponding LBA
    wayIdx = getValidWay(req.range.slpn, tick);

    // We have data to flush
    if (wayIdx != waySize) {
      FTL::Request reqInternal(lineCountInSuperPage);

      reqInternal.reqID = req.reqID;
      reqInternal.reqSubID = req.reqSubID;
      reqInternal.lpn = req.range.slpn / lineCountInSuperPage;
      reqInternal.ioFlag.set(req.range.slpn % lineCountInSuperPage);

      // We have data which is dirty
      if (cacheData[setIdx][wayIdx].dirty) {
        // we have to flush this
        pFTL->write(reqInternal, tick);
      }

      // Invalidate
      cacheData[setIdx][wayIdx].valid = false;

      ret = true;
    }
  }

  return ret;
}

// True when hit
bool GenericCache::trim(Request &req, uint64_t &tick) {
  bool ret = false;
  FTL::Request reqInternal(lineCountInSuperPage);

  debugprint(LOG_ICL_GENERIC_CACHE,
             "TRIM  | REQ %7u-%-4u | LCA %" PRIu64 " | SIZE %" PRIu64,
             req.reqID, req.reqSubID, req.range.slpn, req.length);

  if (useReadCaching || useWriteCaching) {
    uint32_t setIdx = calcSetIndex(req.range.slpn);
    uint32_t wayIdx;

    // Check cache that we have data for corresponding LBA
    wayIdx = getValidWay(req.range.slpn, tick);

    if (wayIdx != waySize) {
      // Invalidate
      cacheData[setIdx][wayIdx].valid = false;
    }
  }

  reqInternal.reqID = req.reqID;
  reqInternal.reqSubID = req.reqSubID;
  reqInternal.lpn = req.range.slpn / lineCountInSuperPage;
  reqInternal.ioFlag.set(req.range.slpn % lineCountInSuperPage);

  // we have to flush this
  pFTL->trim(reqInternal, tick);

  return ret;
}

void GenericCache::format(LPNRange &range, uint64_t &tick) {
  if (useReadCaching || useWriteCaching) {
    uint64_t lpn;
    uint32_t setIdx;
    uint32_t wayIdx;

    for (uint64_t i = 0; i < range.nlp; i++) {
      lpn = range.slpn + i;
      setIdx = calcSetIndex(lpn);
      wayIdx = getValidWay(lpn, tick);

      if (wayIdx != waySize) {
        // Invalidate
        cacheData[setIdx][wayIdx].valid = false;
      }
    }
  }

  // Convert unit
  range.slpn /= lineCountInSuperPage;
  range.nlp = (range.nlp - 1) / lineCountInSuperPage + 1;

  pFTL->format(range, tick);
}

void GenericCache::getStatList(std::vector<Stats> &list, std::string prefix) {
  Stats temp;

  temp.name = prefix + "generic_cache.read.request_count";
  temp.desc = "Read request count";
  list.push_back(temp);

  temp.name = prefix + "generic_cache.read.from_cache";
  temp.desc = "Read requests that served from cache";
  list.push_back(temp);

  temp.name = prefix + "generic_cache.write.request_count";
  temp.desc = "Write request count";
  list.push_back(temp);

  temp.name = prefix + "generic_cache.write.to_cache";
  temp.desc = "Write requests that served to cache";
  list.push_back(temp);
}

void GenericCache::getStatValues(std::vector<double> &values) {
  values.push_back(stat.request[0]);
  values.push_back(stat.cache[0]);
  values.push_back(stat.request[1]);
  values.push_back(stat.cache[1]);
}

void GenericCache::resetStatValues() {
  memset(&stat, 0, sizeof(stat));
}

}  // namespace ICL

}  // namespace SimpleSSD
