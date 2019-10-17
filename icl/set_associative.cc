// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "icl/set_associative.hh"

#include <algorithm>
#include <cstddef>
#include <limits>

#include "util/algorithm.hh"

namespace SimpleSSD::ICL {

SetAssociative::PrefetchTrigger::PrefetchTrigger(const uint64_t c,
                                                 const uint64_t r)
    : prefetchCount(c),
      prefetchRatio(r),
      lastRequestID(std::numeric_limits<uint64_t>::max()),
      requestCounter(0),
      requestCapacity(0),
      lastAddress(0) {}

bool SetAssociative::PrefetchTrigger::trigger(Request &req) {
  if (req.id == lastRequestID) {
    // Just increase values
    requestCapacity++;
  }
  else {
    // New request arrived, check sequential
    if (req.address == lastAddress + 1) {
      requestCounter++;
      requestCapacity++;
    }
    else {
      // Reset
      requestCounter = 0;
      requestCapacity = 0;
    }
  }

  lastAddress = req.address;

  if (requestCounter >= prefetchCount && requestCapacity >= prefetchRatio) {
    return true;
  }

  return false;
}

SetAssociative::SetAssociative(ObjectData &o, FTL::FTL *p)
    : AbstractCache(o, p),
      trigger(
          readConfigUint(Section::InternalCache, Config::Key::PrefetchCount),
          readConfigUint(Section::InternalCache, Config::Key::PrefetchRatio)),
      clock(0),
      requestCounter(0) {
  auto param = pFTL->getInfo();
  auto cacheSize =
      readConfigUint(Section::InternalCache, Config::Key::CacheSize);
  uint8_t limit = 0;

  lineSize = param->pageSize;

  // Make granularity
  evictPages = 1;

  switch ((Config::Granularity)readConfigUint(Section::InternalCache,
                                              Config::Key::EvictMode)) {
    case Config::Granularity::SuperpageLevel:
      limit = param->superpageLevel;

      break;
    case Config::Granularity::AllLevel:
      limit = 4;

      break;
    default:
      panic("Invalid eviction granularity.");

      break;
  }

  for (uint8_t i = 0; i < limit; i++) {
    evictPages *= param->parallelismLevel[i];
  }

  prefetchPages = 1;

  switch ((Config::Granularity)readConfigUint(Section::InternalCache,
                                              Config::Key::PrefetchMode)) {
    case Config::Granularity::SuperpageLevel:
      limit = param->superpageLevel;

      break;
    case Config::Granularity::AllLevel:
      limit = 4;

      break;
    default:
      panic("Invalid prefetch granularity.");

      break;
  }

  for (uint8_t i = 0; i < limit; i++) {
    prefetchPages *= param->parallelismLevel[i];
  }

  // Fully-associated?
  if (waySize == 0) {
    setSize = 1;
    waySize = MAX(cacheSize / lineSize, 1);
  }
  else {
    setSize = MAX(cacheSize / lineSize / waySize, 1);
  }

  cacheSize = (uint64_t)setSize * waySize * lineSize;

  readEnabled =
      readConfigBoolean(Section::InternalCache, Config::Key::EnableReadCache);
  writeEnabled =
      readConfigBoolean(Section::InternalCache, Config::Key::EnableWriteCache);
  prefetchEnabled =
      readConfigBoolean(Section::InternalCache, Config::Key::EnablePrefetch);

  debugprint(
      Log::DebugID::ICL_SetAssociative,
      "CREATE  | Set size %u | Way size %u | Line size %u | Capacity %" PRIu64,
      setSize, waySize, lineSize, cacheSize);
  debugprint(Log::DebugID::ICL_SetAssociative,
             "CREATE  | Eviction granularity %u pages", evictPages);

  // Allocate SRAM for cache metadata
  metaLineSize = sizeof(LPN) + 3;  // PACKED!

  metaAddress = object.sram->allocate(metaLineSize * setSize * waySize);

  // Allocate DRAM for cache data
  dataAddress = object.dram->allocate(cacheSize);

  // Allocate cache for simulation
  cacheMetadata = (Line *)calloc(setSize * waySize, sizeof(Line));

  // Create evict policy
  evictPolicy = (Config::EvictModeType)readConfigUint(Section::InternalCache,
                                                      Config::Key::EvictMode);

  switch (evictPolicy) {
    case Config::EvictModeType::Random:
      mtengine = std::mt19937(rd());
      dist = std::uniform_int_distribution<uint32_t>(0, waySize - 1);

      chooseLine = [this](uint32_t) -> uint32_t { return dist(mtengine); };

      break;
    case Config::EvictModeType::FIFO:
    case Config::EvictModeType::LRU:
      chooseLine = [this](uint32_t set) -> uint32_t {
        Line *wayList = cacheMetadata + set * waySize;
        uint16_t diff = 0;
        uint32_t way = 0;

        // Find line with largest difference
        for (uint32_t i = 0; i < waySize; i++) {
          if (diff > clock - wayList[i].clock) {
            diff = clock - wayList[i].clock;
            way = i;
          }
        }

        return way;
      };

      break;
  }

  memset(&stat, 0, sizeof(stat));

  // Make events
  eventReadPreCPUDone =
      createEvent([this](uint64_t, uint64_t d) { read_findDone(d); },
                  "ICL::SetAssociative::eventReadPreCPUDone");
  eventReadMetaDone =
      createEvent([this](uint64_t, uint64_t d) { read_doftl(d); },
                  "ICL::SetAssociative::eventReadMetaDone");
  eventReadFTLDone =
      createEvent([this](uint64_t, uint64_t d) { read_dodram(d); },
                  "ICL::SetAssociative::eventReadFTLDone");
  eventReadDRAMDone =
      createEvent([this](uint64_t t, uint64_t d) { read_dodma(t, d); },
                  "ICL::SetAssociative::eventReadDRAMDone");
  eventReadDMADone = createEvent([this](uint64_t, uint64_t d) { read_done(d); },
                                 "ICL::SetAssociative::eventReadDMADone");
}

SetAssociative::~SetAssociative() {
  free(cacheMetadata);
}

CacheContext SetAssociative::findRequest(std::list<CacheContext> &queue,
                                         uint64_t tag) {
  for (auto iter = queue.begin(); iter != queue.end(); ++iter) {
    if (iter->id == tag) {
      auto ret = std::move(*iter);

      queue.erase(iter);

      return ret;
    }
  }

  panic("Failed to find request in queue.");
}

void SetAssociative::read_find(Request &&req) {
  CPU::Function fstat = CPU::initFunction();
  CacheContext ctx(req);

  ctx.id = requestCounter++;
  ctx.submittedAt = getTick();

  debugprint(Log::DebugID::ICL_SetAssociative,
             "READ  | REQ %7u | LPN %" PRIx64 "h | SIZE %" PRIu64, ctx.req.id,
             ctx.req.address, lineSize - ctx.req.skipFront - ctx.req.skipEnd);

  if (LIKELY(readEnabled)) {
    ctx.setIdx = getSetIndex(ctx.req.address);

    if (getValidWay(ctx.setIdx, ctx.wayIdx)) {  // Hit
      auto line = getLine(ctx.setIdx, ctx.wayIdx);

      if (line->rpending) {
        // Request is pending
        ctx.status = LineStatus::ReadHitPending;
      }
      else {
        // If write is pending, we can just read it
        line->tag = ctx.req.address;
        line->valid = true;

        if (evictPolicy == Config::EvictModeType::LRU) {
          // Update clock at access
          line->clock = clock;
        }

        if (!line->wpending) {
          // If write is pending, dirty bit will becom false when write done
          line->dirty = false;
        }

        ctx.status = LineStatus::ReadHit;
      }
    }
    else if (getEmptyWay(ctx.setIdx, ctx.wayIdx)) {  // Cold miss
      auto line = getLine(ctx.setIdx, ctx.wayIdx);

      line->tag = ctx.req.address;
      line->clock = clock;
      line->valid = true;
      line->dirty = false;
      line->rpending = true;  // Now, we will read this line from FTL
      line->wpending = false;

      ctx.status = LineStatus::ReadColdMiss;
    }
    else {  // Conflict miss
      evict(ctx.setIdx);

      ctx.status = LineStatus::ReadMiss;
    }

    readMetaQueue.emplace_back(ctx);

    if (trigger.trigger(ctx.req) && prefetchEnabled) {
      prefetch(ctx.req.address + 1, ctx.req.address + prefetchPages);
    }

    scheduleFunction(CPU::CPUGroup::InternalCache, eventReadPreCPUDone, ctx.id,
                     fstat);
  }
  else {
    ctx.status = LineStatus::ReadColdMiss;

    // Set arbitraray set ID to pervent DRAM write queue hit
    ctx.wayIdx = ctx.id % waySize;
    ctx.setIdx = (ctx.id / waySize) % setSize;

    scheduleNow(eventReadMetaDone, ctx.id);
  }
}

void SetAssociative::read_findDone(uint64_t tag) {
  auto ctx = findRequest(readMetaQueue, tag);

  // Add metadata access latency (for one set - all way lines)
  object.sram->read(metaAddress + ctx.setIdx * waySize * metaLineSize,
                    metaLineSize * waySize, eventReadMetaDone, tag);

  readMetaQueue.emplace_back(ctx);
}

void SetAssociative::read_doftl(uint64_t tag) {
  auto ctx = findRequest(readMetaQueue, tag);

  switch (ctx.status) {
    case LineStatus::ReadHitPending:
    case LineStatus::ReadHit:
      // Skip FTL
      scheduleNow(eventReadDRAMDone, tag);

      readDRAMQueue.emplace_back(ctx);

      return;
    case LineStatus::ReadMiss:
      // Evict first
      readEvictQueue.emplace_back(ctx);

      // We should not push to readFTLQueue
      return;
    case LineStatus::ReadColdMiss:
    case LineStatus::Prefetch:
      // Do read
      pFTL->submit(FTL::Request(ctx.req.id, eventReadFTLDone, ctx.id,
                                FTL::Operation::Read, ctx.req.address,
                                ctx.req.buffer));

      break;
  }

  readFTLQueue.emplace_back(ctx);
}

void SetAssociative::read_dodram(uint64_t tag) {
  auto ctx = findRequest(readFTLQueue, tag);

  // Add NVM -> DRAM DMA latency
  object.dram->write(
      dataAddress + (ctx.setIdx * waySize + ctx.wayIdx) * lineSize, lineSize,
      eventReadDRAMDone, tag);

  readDRAMQueue.emplace_back(ctx);
}

void SetAssociative::read_dodma(uint64_t now, uint64_t tag) {
  auto ctx = findRequest(readDRAMQueue, tag);

  // Read done
  auto line = getLine(ctx.setIdx, ctx.wayIdx);

  line->rpending = false;

  ctx.finishedAt = now;

  if (ctx.status == LineStatus::Prefetch) {
    // We will not transfer this request to host (complete here)
    debugprint(Log::DebugID::ICL_SetAssociative,
               "READ  | PREFETCH    | LPN %" PRIx64 "h | %" PRIu64 " - %" PRIu64
               "(%" PRIu64 ")",
               ctx.req.address, ctx.submittedAt, ctx.finishedAt,
               ctx.finishedAt - ctx.submittedAt);
  }
  else {
    switch (ctx.status) {
      case LineStatus::ReadHit:
        debugprint(Log::DebugID::ICL_SetAssociative,
                   "READ  | REQ %7u | Cache hit (%u, %u) | %" PRIu64
                   " - %" PRIu64 "(%" PRIu64 ")",
                   ctx.req.id, ctx.setIdx, ctx.wayIdx, ctx.submittedAt,
                   ctx.finishedAt, ctx.finishedAt - ctx.submittedAt);

        break;
      case LineStatus::ReadColdMiss:
      case LineStatus::ReadMiss:
        debugprint(Log::DebugID::ICL_SetAssociative,
                   "READ  | REQ %7u | Cache miss (%u, %u) | %" PRIu64
                   " - %" PRIu64 "(%" PRIu64 ")",
                   ctx.req.id, ctx.setIdx, ctx.wayIdx, ctx.submittedAt,
                   ctx.finishedAt, ctx.finishedAt - ctx.submittedAt);

        break;
      default:
        // No log here (Prefetch and ReadHitPending)
        break;
    }

    // Add DRAM -> PCIe DMA latency
    // Actually, this should be performed in HIL layer -- but they don't know
    // which memory address to read. All read to memory will hit in write queue
    // of DRAM controller (It should be negliegible).
    object.dram->read(
        dataAddress + (ctx.setIdx * waySize + ctx.wayIdx) * lineSize, lineSize,
        eventReadDMADone, tag);

    readDMAQueue.emplace_back(ctx);
  }

  // At this point, we can handle pending request
  for (auto iter = readPendingQueue.begin(); iter != readPendingQueue.end();) {
    auto line = getLine(iter->setIdx, iter->wayIdx);

    if (line->rpending == false) {
      // Now this request can complete
      if (iter->status == LineStatus::ReadHitPending) {
        iter->status = LineStatus::ReadHit;
        iter->finishedAt = now;

        debugprint(Log::DebugID::ICL_SetAssociative,
                   "READ  | REQ %7u | Cache hit delayed (%u, %u) | %" PRIu64
                   " - %" PRIu64 "(%" PRIu64 ")",
                   iter->req.id, iter->setIdx, iter->wayIdx, iter->submittedAt,
                   iter->finishedAt, iter->finishedAt - iter->submittedAt);

        // Add DRAM -> PCIe DMA latency
        object.dram->read(
            dataAddress + (iter->setIdx * waySize + iter->wayIdx) * lineSize,
            lineSize, eventReadDMADone, iter->id);

        readDMAQueue.emplace_back(*iter);
      }
      else {
        // TODO: write pending
      }

      readPendingQueue.erase(iter);

      break;
    }
  }
}

void SetAssociative::read_done(uint64_t tag) {
  auto ctx = findRequest(readDMAQueue, tag);

  scheduleNow(ctx.req.eid, ctx.req.data);
}

void SetAssociative::write_find(Request &&req) {}

void SetAssociative::prefetch(LPN begin, LPN end) {
  CPU::Function fstat = CPU::initFunction();
  uint32_t setIdx;
  uint32_t wayIdx;

  // Check which page to read
  for (LPN i = begin; i < end; i++) {
    setIdx = getSetIndex(i);

    if (getValidWay(setIdx, wayIdx)) {
      // We have valid data at current LPN i (Don't need to read)
    }
    else if (getEmptyWay(setIdx, wayIdx)) {
      // No valid way and current set has empty line
      auto line = getLine(setIdx, wayIdx);

      // Mark as prefetch
      line->tag = i;
      line->clock = clock;
      line->dirty = false;
      line->valid = true;
      line->rpending = true;
      line->wpending = false;

      // Make request
      CacheContext ctx;

      ctx.id = requestCounter++;
      ctx.req.address = i;
      ctx.setIdx = setIdx;
      ctx.wayIdx = wayIdx;
      ctx.status = LineStatus::Prefetch;
      ctx.submittedAt = getTick();

      readMetaQueue.emplace_back(ctx);

      scheduleFunction(CPU::CPUGroup::InternalCache, eventReadPreCPUDone,
                       ctx.id, fstat);
    }
    else {
      // No valid way and no empty line
      // We will not read this LPN i (To not make additional write in prefetch)
    }
  }
}

void SetAssociative::enqueue(Request &&req) {
  // Increase clock
  clock++;

  switch (req.opcode) {
    case Operation::Read:
      read_find(std::move(req));

      break;
    case Operation::Write:
      write_find(std::move(req));

      break;
    case Operation::Flush:
      flush_find(std::move(req));

      break;
    case Operation::Trim:
    case Operation::Format:
      invalidate_find(std::move(req));

      break;
    default:
      panic("Unexpected opcode.");
  }
}

void SetAssociative::getStatList(std::vector<Stat> &list,
                                 std::string prefix) noexcept {
  Stat temp;

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

void SetAssociative::getStatValues(std::vector<double> &values) noexcept {
  values.push_back(stat.request[0]);
  values.push_back(stat.cache[0]);
  values.push_back(stat.request[1]);
  values.push_back(stat.cache[1]);
}

void SetAssociative::resetStatValues() noexcept {
  memset(&stat, 0, sizeof(stat));
}

void SetAssociative::createCheckpoint(std::ostream &) const noexcept {}

void SetAssociative::restoreCheckpoint(std::istream &) noexcept {}

}  // namespace SimpleSSD::ICL
