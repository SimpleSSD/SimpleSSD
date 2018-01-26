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
#include <limits>

#include "log/trace.hh"
#include "util/algorithm.hh"

namespace SimpleSSD {

namespace ICL {

EvictData::EvictData() : setIdx(0), wayIdx(0), tag(0) {}

GenericCache::GenericCache(ConfigReader *c, FTL::FTL *f)
    : AbstractCache(c, f),
      lineCountInSuperPage(f->getInfo()->ioUnitInPage),
      superPageSize(f->getInfo()->pageSize),
      waySize(c->iclConfig.readUint(ICL_WAY_SIZE)),
      lineSize(superPageSize / lineCountInSuperPage),
      lineCountInMaxIO(f->getInfo()->pageCountToMaxPerf * lineCountInSuperPage),
      prefetchIOCount(c->iclConfig.readUint(ICL_PREFETCH_COUNT)),
      prefetchIORatio(c->iclConfig.readFloat(ICL_PREFETCH_RATIO)),
      useReadCaching(c->iclConfig.readBoolean(ICL_USE_READ_CACHE)),
      useWriteCaching(c->iclConfig.readBoolean(ICL_USE_WRITE_CACHE)),
      useReadPrefetch(c->iclConfig.readBoolean(ICL_USE_READ_PREFETCH)),
      gen(rd()),
      dist(std::uniform_int_distribution<>(0, waySize - 1)) {
  uint64_t cacheSize = c->iclConfig.readUint(ICL_CACHE_SIZE);

  setSize = MAX(cacheSize / lineSize / waySize, 1);

  // Set size should multiples of lineCountInSuperPage
  uint32_t left = setSize % lineCountInMaxIO;

  if (left) {
    if (left * 2 > lineCountInMaxIO) {
      setSize = (setSize / lineCountInMaxIO + 1) * lineCountInMaxIO;
    }
    else {
      setSize -= left;
    }

    if (setSize < 1) {
      setSize = lineCountInMaxIO;
    }
  }

  Logger::debugprint(
      Logger::LOG_ICL_GENERIC_CACHE,
      "CREATE  | Set size %u | Way size %u | Line size %u | Capacity %" PRIu64,
      setSize, waySize, lineSize, (uint64_t)setSize * waySize * lineSize);
  Logger::debugprint(
      Logger::LOG_ICL_GENERIC_CACHE,
      "CREATE  | line count in super page %u | line count in max I/O %u",
      lineCountInSuperPage, lineCountInMaxIO);

  // TODO: replace this with DRAM model
  pTiming = c->iclConfig.getDRAMTiming();
  pStructure = c->iclConfig.getDRAMStructure();

  ppCache.resize(setSize);

  for (uint32_t i = 0; i < setSize; i++) {
    ppCache[i] = std::vector<Line>(waySize, Line());
  }

  lastRequest.reqID = 1;
  prefetchEnabled = false;
  hitCounter = 0;
  accessCounter = 0;

  // Set evict policy functional
  policy = (EVICT_POLICY)c->iclConfig.readInt(ICL_EVICT_POLICY);

  switch (policy) {
    case POLICY_RANDOM:
      evictFunction = [this](uint32_t setIdx) -> uint32_t { return dist(gen); };

      break;
    case POLICY_FIFO:
      evictFunction = [this](uint32_t setIdx) -> uint32_t {
        uint32_t wayIdx = 0;
        uint64_t min = std::numeric_limits<uint64_t>::max();

        for (uint32_t i = 0; i < waySize; i++) {
          if (ppCache[setIdx][i].insertedAt < min) {
            min = ppCache[setIdx][i].insertedAt;
            wayIdx = i;
          }
        }

        return wayIdx;
      };

      break;
    case POLICY_LEAST_RECENTLY_USED:
      evictFunction = [this](uint32_t setIdx) -> uint32_t {
        uint32_t wayIdx = 0;
        uint64_t min = std::numeric_limits<uint64_t>::max();

        for (uint32_t i = 0; i < waySize; i++) {
          if (ppCache[setIdx][i].lastAccessed < min) {
            min = ppCache[setIdx][i].lastAccessed;
            wayIdx = i;
          }
        }

        return wayIdx;
      };

      break;
    default:
      evictFunction = [](uint32_t setIdx) -> uint32_t { return 0; };
      break;
  }
}

GenericCache::~GenericCache() {}

uint32_t GenericCache::calcSet(uint64_t lca) {
  return lca % setSize;
}

uint32_t GenericCache::getEmptyWay(uint32_t setIdx) {
  uint32_t retIdx = waySize;
  uint64_t minInsertedAt = std::numeric_limits<uint64_t>::max();

  for (uint32_t wayIdx = 0; wayIdx < waySize; wayIdx++) {
    Line &line = ppCache[setIdx][wayIdx];

    if (!line.valid) {
      if (minInsertedAt > line.insertedAt) {
        minInsertedAt = line.insertedAt;
        retIdx = wayIdx;
      }
    }
  }

  return retIdx;
}

uint32_t GenericCache::getValidWay(uint64_t lca) {
  uint32_t setIdx = calcSet(lca);
  uint32_t wayIdx;

  for (wayIdx = 0; wayIdx < waySize; wayIdx++) {
    Line &line = ppCache[setIdx][wayIdx];

    if (line.valid && line.tag == lca) {
      break;
    }
  }

  return wayIdx;
}

uint32_t GenericCache::getVictimWay(uint64_t lca) {
  uint32_t setIdx = calcSet(lca);
  uint32_t wayIdx;

  wayIdx = getValidWay(lca);

  if (wayIdx == waySize) {
    wayIdx = getEmptyWay(setIdx);

    if (wayIdx == waySize) {
      wayIdx = evictFunction(setIdx);
    }
  }

  return wayIdx;
}

uint32_t GenericCache::getDirtyEntryCount(uint64_t lpn,
                                          std::vector<EvictData> &list) {
  uint64_t lca = lpn * lineCountInSuperPage;
  uint32_t counter = 0;
  EvictData data;

  list.clear();

  for (uint32_t i = 0; i < lineCountInSuperPage; i++) {
    uint32_t setIdx = calcSet(lca + i);
    uint32_t wayIdx = getValidWay(lca + i);

    if (wayIdx != waySize) {
      if (ppCache[setIdx][wayIdx].dirty) {
        data.setIdx = setIdx;
        data.wayIdx = wayIdx;

        list.push_back(data);

        counter++;
      }
    }
  }

  return counter;
}

bool GenericCache::compareEvictList(std::vector<EvictData> &a,
                                    std::vector<EvictData> &b) {
  bool ret = true;  // True means a should evicted
  static auto maxInsertedAt = [this](std::vector<EvictData> &list) -> uint32_t {
    uint64_t max = 0;

    for (auto &iter : list) {
      if (ppCache[iter.setIdx][iter.wayIdx].insertedAt > max) {
        max = ppCache[iter.setIdx][iter.wayIdx].insertedAt;
      }
    }

    return max;
  };
  static auto maxLastAccessed =
      [this](std::vector<EvictData> &list) -> uint32_t {
    uint64_t max = 0;

    for (auto &iter : list) {
      if (ppCache[iter.setIdx][iter.wayIdx].lastAccessed > max) {
        max = ppCache[iter.setIdx][iter.wayIdx].lastAccessed;
      }
    }

    return max;
  };

  switch (policy) {
    case POLICY_RANDOM:
      if (dist(gen) > waySize / 2) {
        ret = false;
      }

      break;
    case POLICY_FIFO:
      if (maxInsertedAt(a) > maxInsertedAt(b)) {
        ret = false;
      }

      break;
    case POLICY_LEAST_RECENTLY_USED:
      if (maxLastAccessed(a) > maxLastAccessed(b)) {
        ret = false;
      }
      break;
  }

  return ret;
}

uint64_t GenericCache::calculateDelay(uint64_t bytesize) {
  uint64_t pageCount =
      (bytesize > 0) ? (bytesize - 1) / pStructure->pageSize + 1 : 0;
  uint64_t pageFetch = pTiming->tRP + pTiming->tRCD + pTiming->tCL;
  double bandwidth =
      2.0 * pStructure->busWidth * pStructure->channel / 8.0 / pTiming->tCK;

  return (uint64_t)(pageFetch + pageCount * pStructure->pageSize / bandwidth);
}

void GenericCache::evictVictim(std::vector<EvictData> &list, bool isRead,
                               uint64_t &tick) {
  static uint64_t lat = calculateDelay(sizeof(Line) + lineSize) * waySize;
  std::vector<FTL::Request> reqList;

  if (list.size() == 0) {
    Logger::panic("Evict list is empty");
  }

  // Collect lines to write
  Logger::debugprint(Logger::LOG_ICL_GENERIC_CACHE,
                     "----- | Flushing set %u - %u (%u)", list.front().setIdx,
                     list.back().setIdx, list.size());

  for (auto &iter : list) {
    auto &line = ppCache[iter.setIdx][iter.wayIdx];

    if (line.valid && line.dirty && (!isRead || line.tag != iter.tag)) {
      FTL::Request req(lineCountInSuperPage);

      Logger::debugprint(Logger::LOG_ICL_GENERIC_CACHE,
                         "----- | Flush (%u, %u) | LBA %" PRIu64, iter.setIdx,
                         iter.wayIdx, line.tag);

      // We need to flush this
      req.lpn = line.tag / lineCountInSuperPage;
      req.ioFlag.set(line.tag % lineCountInSuperPage);

      reqList.push_back(req);
    }
  }

  if (reqList.size() == 0) {
    Logger::debugprint(Logger::LOG_ICL_GENERIC_CACHE,
                       "----- | Cache line is clean, no need to flush");

    tick += lat;
  }
  else {
    uint64_t size = reqList.size();

    // Merge same lpn for performance
    for (uint64_t i = 0; i < size; i++) {
      auto &reqi = reqList.at(i);

      if (reqi.reqID) {  // reqID should zero (internal I/O)
        continue;
      }

      for (uint64_t j = i + 1; j < size; j++) {
        auto &reqj = reqList.at(j);

        if (reqi.lpn == reqj.lpn && reqj.reqID == 0) {
          reqj.reqID = 1;
          reqi.ioFlag |= reqj.ioFlag;
        }
      }
    }

    // Do write
    uint64_t beginAt;
    uint64_t finishedAt = tick;

    for (auto &iter : reqList) {
      if (iter.reqID == 0) {
        beginAt = tick;

        pFTL->write(iter, beginAt);

        finishedAt = MAX(finishedAt, beginAt);
      }
    }

    tick = finishedAt;
  }

  // Update line
  for (auto &iter : list) {
    auto &line = ppCache[iter.setIdx][iter.wayIdx];

    if (isRead) {
      // On read, make this cacheline valid and clean
      line.valid = true;
    }
    else {
      // On write, make this cacheline empty
      line.valid = false;
    }

    line.dirty = false;
    line.tag = iter.tag;
    line.insertedAt = tick;
    line.lastAccessed = tick;
  }
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
          (float)accessCounter / superPageSize > prefetchIORatio) {
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

// True when hit
bool GenericCache::read(Request &req, uint64_t &tick) {
  bool ret = false;

  Logger::debugprint(Logger::LOG_ICL_GENERIC_CACHE,
                     "READ  | REQ %7u-%-4u | LCA %" PRIu64 " | SIZE %" PRIu64,
                     req.reqID, req.reqSubID, req.range.slpn, req.length);

  if (useReadCaching) {
    uint32_t setIdx = calcSet(req.range.slpn);
    uint32_t wayIdx;
    static uint64_t lat = calculateDelay(sizeof(Line) + lineSize) * waySize;

    // Check prefetch
    if (useReadPrefetch) {
      checkPrefetch(req);
    }

    // Check cache that we have data for corresponding LCA
    wayIdx = getValidWay(req.range.slpn);

    if (wayIdx != waySize) {
      uint64_t arrived = tick;

      // Wait for cache
      if (tick < ppCache[setIdx][wayIdx].insertedAt) {
        tick = ppCache[setIdx][wayIdx].insertedAt;
      }

      // Update cache line
      ppCache[setIdx][wayIdx].lastAccessed = tick;

      // Add tDRAM
      tick += lat;

      Logger::debugprint(Logger::LOG_ICL_GENERIC_CACHE,
                         "READ  | Cache hit at (%u, %u) | %" PRIu64
                         " - %" PRIu64 " (%" PRIu64 ")",
                         setIdx, wayIdx, arrived, tick, tick - arrived);

      // Data served from DRAM
      ret = true;
    }
    else {
      FTL::Request reqInternal(lineCountInSuperPage, req);
      std::vector<EvictData> list;
      EvictData data;
      uint64_t beginAt;
      uint64_t finishedAt = tick;

      if (prefetchEnabled) {
        static const uint32_t mapSize = lineCountInMaxIO / lineCountInSuperPage;

        // Read one superpage except already read pages
        for (uint32_t count = 0; count < mapSize; count++) {
          reqInternal.ioFlag.set();

          for (uint32_t i = 0; i < lineCountInSuperPage; i++) {
            data.tag = reqInternal.lpn * lineCountInSuperPage + i;

            if (getValidWay(data.tag) == waySize) {
              data.setIdx = calcSet(data.tag);
              data.wayIdx = getVictimWay(data.tag);

              list.push_back(data);
            }
            else {
              reqInternal.ioFlag.set(i, false);
            }
          }

          beginAt = tick;

          pFTL->read(reqInternal, beginAt);

          if (reqInternal.lpn == req.range.slpn / lineCountInSuperPage) {
            finishedAt = beginAt;
          }

          reqInternal.lpn++;
        }
      }
      else {
        data.tag = req.range.slpn;
        data.setIdx = setIdx;
        data.wayIdx = getVictimWay(data.tag);

        list.push_back(data);

        pFTL->read(reqInternal, finishedAt);
      }

      // Flush collected lines
      evictVictim(list, true, tick);

      tick = finishedAt;
    }
  }
  else {
    FTL::Request reqInternal(lineCountInSuperPage, req);

    pFTL->read(reqInternal, tick);
  }

  return ret;
}

// True when cold-miss/hit
bool GenericCache::write(Request &req, uint64_t &tick) {
  bool ret = false;

  Logger::debugprint(Logger::LOG_ICL_GENERIC_CACHE,
                     "WRITE | REQ %7u-%-4u | LCA %" PRIu64 " | SIZE %" PRIu64,
                     req.reqID, req.reqSubID, req.range.slpn, req.length);

  if (useWriteCaching) {
    uint32_t setIdx = calcSet(req.range.slpn);
    uint32_t wayIdx;
    static uint64_t lat = calculateDelay(sizeof(Line) + lineSize) * waySize;

    // Check cache that we have data for corresponding LCA
    wayIdx = getValidWay(req.range.slpn);

    if (wayIdx != waySize) {
      uint64_t arrived = tick;

      // Wait for cache
      if (tick < ppCache[setIdx][wayIdx].insertedAt) {
        tick = ppCache[setIdx][wayIdx].insertedAt;
      }

      // Update cache line
      ppCache[setIdx][wayIdx].insertedAt = tick;
      ppCache[setIdx][wayIdx].lastAccessed = tick;
      ppCache[setIdx][wayIdx].dirty = true;

      // Add tDRAM
      tick += lat;

      Logger::debugprint(Logger::LOG_ICL_GENERIC_CACHE,
                         "WRITE | Cache hit at (%u, %u) | %" PRIu64
                         " - %" PRIu64 " (%" PRIu64 ")",
                         setIdx, wayIdx, arrived, tick, tick - arrived);

      // Data written to DRAM
      ret = true;
    }
    else {
      // Check cache that we have empty slot
      wayIdx = getEmptyWay(setIdx);

      if (wayIdx != waySize) {
        uint64_t arrived = tick;

        // Wait for cache
        if (tick < ppCache[setIdx][wayIdx].insertedAt) {
          tick = ppCache[setIdx][wayIdx].insertedAt;
        }

        // Update cache line
        ppCache[setIdx][wayIdx].valid = true;
        ppCache[setIdx][wayIdx].dirty = true;
        ppCache[setIdx][wayIdx].tag = req.range.slpn;
        ppCache[setIdx][wayIdx].insertedAt = tick;
        ppCache[setIdx][wayIdx].lastAccessed = tick;

        // Add tDRAM
        tick += lat;

        Logger::debugprint(Logger::LOG_ICL_GENERIC_CACHE,
                           "WRITE | Cache miss at (%u, %u) | %" PRIu64
                           " - %" PRIu64 " (%" PRIu64 ")",
                           setIdx, wayIdx, arrived, tick, tick - arrived);

        // Data written to DRAM
        ret = true;
      }
      else {
        static const uint32_t mapSize = lineCountInMaxIO / lineCountInSuperPage;
        uint32_t count;
        uint32_t idxToReturn = 0;
        std::vector<EvictData> tempList;

        // Collect LPNs we can evict
        std::vector<std::pair<uint32_t, std::vector<EvictData>>> maxList(
            mapSize, {0, std::vector<EvictData>()});

        // For one super page
        for (uint32_t set = 0; set < setSize; set += lineCountInSuperPage) {
          std::vector<uint64_t> lpns;
          uint32_t mapOffset = (set / lineCountInSuperPage) % mapSize;
          bool force = false;

          // Check this super page includes current setIdx will be evicted
          if (set <= setIdx && setIdx < set + lineCountInSuperPage) {
            force = true;
            idxToReturn = mapOffset;
          }

          // Collect all LPNs exist on current super page
          for (uint32_t i = 0; i < lineCountInSuperPage; i++) {
            for (uint32_t j = 0; j < waySize; j++) {
              Line &line = ppCache[set + i][j];

              if (line.valid) {
                lpns.push_back(line.tag / lineCountInSuperPage);
              }
            }
          }

          // Remove duplicates
          std::sort(lpns.begin(), lpns.end());
          auto last = std::unique(lpns.begin(), lpns.end());

          // Check this super page includes current setIdx will be evicted
          if (force) {
            maxList.at(mapOffset).first = 0;
          }

          for (auto iter = lpns.begin(); iter != last; iter++) {
            count = getDirtyEntryCount(*iter, tempList);

            // tempList should contain current setIdx
            if (force) {
              bool exist = false;

              for (auto &iter : tempList) {
                if (iter.setIdx == setIdx) {
                  exist = true;

                  break;
                }
              }

              if (!exist) {
                continue;
              }
            }

            if (maxList.at(mapOffset).first < count) {
              maxList.at(mapOffset).first = count;
              maxList.at(mapOffset).second = tempList;
            }
            else if (maxList.at(mapOffset).first == count) {
              if (compareEvictList(tempList, maxList.at(mapOffset).second)) {
                maxList.at(mapOffset).second = tempList;
              }
            }
          }

          if (force) {
            maxList.at(mapOffset).first = std::numeric_limits<uint32_t>::max();

            if (maxList.at(mapOffset).second.size() == 0) {
              EvictData data;

              data.tag = req.range.slpn;
              data.setIdx = setIdx;
              data.wayIdx = getVictimWay(data.tag);

              maxList.at(mapOffset).second.push_back(data);
            }
          }

          lpns.clear();
        }

        // Evict
        uint64_t beginAt;
        uint64_t finishedAt = tick;

        for (uint32_t i = 0; i < mapSize; i++) {
          if (maxList.at(i).second.size() > 0) {
            beginAt = tick;

            evictVictim(maxList.at(i).second, false, beginAt);

            if (i == idxToReturn) {
              finishedAt = beginAt;
            }
          }
        }

        tick = finishedAt;

        // Update cacheline
        wayIdx = getEmptyWay(setIdx);

        if (wayIdx != waySize) {
          ppCache[setIdx][wayIdx].valid = true;
          ppCache[setIdx][wayIdx].dirty = true;
          ppCache[setIdx][wayIdx].tag = req.range.slpn;
          ppCache[setIdx][wayIdx].insertedAt = tick;
          ppCache[setIdx][wayIdx].lastAccessed = tick;
        }
        else {
          Logger::panic("No space to write data in set %u", setIdx);
        }
      }
    }
  }
  else {
    FTL::Request reqInternal(lineCountInSuperPage, req);

    pFTL->write(reqInternal, tick);
  }

  return ret;
}

// True when hit
bool GenericCache::flush(Request &req, uint64_t &tick) {
  bool ret = false;

  if (useReadCaching || useWriteCaching) {
    uint32_t setIdx = calcSet(req.range.slpn);
    uint32_t wayIdx;

    // Check cache that we have data for corresponding LBA
    wayIdx = getValidWay(req.range.slpn);

    // We have data to flush
    if (wayIdx != waySize) {
      FTL::Request reqInternal(lineCountInSuperPage);

      reqInternal.reqID = req.reqID;
      reqInternal.reqSubID = req.reqSubID;
      reqInternal.lpn = req.range.slpn / lineCountInSuperPage;
      reqInternal.ioFlag.set(req.range.slpn % lineCountInSuperPage);

      // We have data which is dirty
      if (ppCache[setIdx][wayIdx].dirty) {
        // we have to flush this
        pFTL->write(reqInternal, tick);
      }

      // invalidate
      ppCache[setIdx][wayIdx].valid = false;
    }
  }

  return ret;
}

// True when hit
bool GenericCache::trim(Request &req, uint64_t &tick) {
  bool ret = false;
  FTL::Request reqInternal(lineCountInSuperPage);

  Logger::debugprint(Logger::LOG_ICL_GENERIC_CACHE,
                     "TRIM  | REQ %7u-%-4u | LCA %" PRIu64 " | SIZE %" PRIu64,
                     req.reqID, req.reqSubID, req.range.slpn, req.length);

  if (useReadCaching || useWriteCaching) {
    uint32_t setIdx = calcSet(req.range.slpn);
    uint32_t wayIdx;

    // Check cache that we have data for corresponding LBA
    wayIdx = getValidWay(req.range.slpn);

    if (wayIdx != waySize) {
      // invalidate
      ppCache[setIdx][wayIdx].valid = false;
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
      setIdx = calcSet(lpn);
      wayIdx = getValidWay(lpn);

      if (wayIdx != waySize) {
        // invalidate
        ppCache[setIdx][wayIdx].valid = false;
      }
    }
  }

  // Convert unit
  range.slpn /= lineCountInSuperPage;
  range.nlp = (range.nlp - 1) / lineCountInSuperPage + 1;

  pFTL->format(range, tick);
}

}  // namespace ICL

}  // namespace SimpleSSD
