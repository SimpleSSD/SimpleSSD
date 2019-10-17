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
      requestCounter(0),
      trigger(
          readConfigUint(Section::InternalCache, Config::Key::PrefetchCount),
          readConfigUint(Section::InternalCache, Config::Key::PrefetchRatio)),
      clock(0) {
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
  eventWritePreCPUDone =
      createEvent([this](uint64_t, uint64_t d) { write_findDone(d); },
                  "ICL::SetAssociative::eventWritePreCPUDone");
  eventWriteMetaDone =
      createEvent([this](uint64_t, uint64_t d) { write_dodram(d); },
                  "ICL::SetAssociative::eventWriteMetaDone");
  eventWriteDRAMDone =
      createEvent([this](uint64_t t, uint64_t d) { write_done(t, d); },
                  "ICL::SetAssociative::eventWriteDRAMDone");
  eventEvictDRAMDone =
      createEvent([this](uint64_t, uint64_t d) { evict_doftl(d); },
                  "ICL::SetAssociative::eventEvictDRAMDone");
  eventEvictFTLDone =
      createEvent([this](uint64_t t, uint64_t d) { evict_done(t, d); },
                  "ICL::SetAssociative::eventEvictFTLDone");
  eventFlushPreCPUDone =
      createEvent([this](uint64_t, uint64_t d) { flush_findDone(d); },
                  "ICL::SetAssociative::eventFlushPreCPUDone");
  eventFlushMetaDone =
      createEvent([this](uint64_t, uint64_t d) { flush_doevict(d); },
                  "ICL::SetAssociative::eventFlushMetaDone");
  eventInvalidatePreCPUDone =
      createEvent([this](uint64_t, uint64_t d) { invalidate_findDone(d); },
                  "ICL::SetAssociative::eventInvalidatePreCPUDone");
  eventInvalidateMetaDone =
      createEvent([this](uint64_t, uint64_t d) { invalidate_doftl(d); },
                  "ICL::SetAssociative::eventInvalidateMetaDone");
  eventInvalidateFTLDone =
      createEvent([this](uint64_t t, uint64_t d) { invalidate_done(t, d); },
                  "ICL::SetAssociative::eventInvalidateFTLDone");
}

SetAssociative::~SetAssociative() {
  free(cacheMetadata);
}

uint32_t SetAssociative::getSetIndex(LPN lpn) {
  return lpn % setSize;
}

bool SetAssociative::getValidWay(LPN lpn, uint32_t setIdx, uint32_t &wayIdx) {
  auto line = getLine(setIdx, 0);

  for (wayIdx = 0; wayIdx < waySize; wayIdx++) {
    if (line[wayIdx].tag == lpn) {
      return true;
    }
  }

  return false;
}

bool SetAssociative::getEmptyWay(uint32_t setIdx, uint32_t &wayIdx) {
  auto line = getLine(setIdx, 0);

  for (wayIdx = 0; wayIdx < waySize; wayIdx++) {
    if (!line[wayIdx].valid) {
      return true;
    }
  }

  return false;
}

SetAssociative::Line *SetAssociative::getLine(uint32_t setIdx,
                                              uint32_t wayIdx) {
  return cacheMetadata + (setIdx * waySize + wayIdx);
}

SetAssociative::CacheContext SetAssociative::findRequest(
    std::list<CacheContext> &queue, uint64_t tag) {
  for (auto iter = queue.begin(); iter != queue.end(); ++iter) {
    if (iter->id == tag) {
      auto ret = std::move(*iter);

      queue.erase(iter);

      return ret;
    }
  }

  panic("Failed to find request in queue.");

  // Not reached
  return CacheContext();
}

void SetAssociative::read_find(Request &&req) {
  CPU::Function fstat = CPU::initFunction();
  CacheContext ctx(req);

  ctx.id = requestCounter++;
  ctx.submittedAt = getTick();

  stat.request[0]++;

  debugprint(Log::DebugID::ICL_SetAssociative,
             "READ   | REQ %7u | LPN %" PRIx64 "h | SIZE %" PRIu64, ctx.req.id,
             ctx.req.address, lineSize - ctx.req.skipFront - ctx.req.skipEnd);

  if (LIKELY(readEnabled)) {
    ctx.setIdx = getSetIndex(ctx.req.address);

    if (getValidWay(ctx.req.address, ctx.setIdx, ctx.wayIdx)) {  // Hit
      auto line = getLine(ctx.setIdx, ctx.wayIdx);

      stat.cache[0]++;

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

    readMetaQueue.emplace_back(ctx);
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
      evictQueue.emplace_back(ctx);

      // We should not push to readFTLQueue
      return;
    case LineStatus::ReadColdMiss:
    case LineStatus::Prefetch:
      // Do read
      pFTL->submit(FTL::Request(ctx.req.id, eventReadFTLDone, ctx.id,
                                FTL::Operation::Read, ctx.req.address,
                                ctx.req.buffer));

      break;
    default:
      panic("Unexpected line status.");

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
               "READ   | PREFETCH    | LPN %" PRIx64 "h | %" PRIu64
               " - %" PRIu64 "(%" PRIu64 ")",
               ctx.req.address, ctx.submittedAt, ctx.finishedAt,
               ctx.finishedAt - ctx.submittedAt);
  }
  else {
    switch (ctx.status) {
      case LineStatus::ReadHit:
        debugprint(Log::DebugID::ICL_SetAssociative,
                   "READ   | REQ %7u | Cache hit (%u, %u) | %" PRIu64
                   " - %" PRIu64 "(%" PRIu64 ")",
                   ctx.req.id, ctx.setIdx, ctx.wayIdx, ctx.submittedAt,
                   ctx.finishedAt, ctx.finishedAt - ctx.submittedAt);

        break;
      case LineStatus::ReadColdMiss:
      case LineStatus::ReadMiss:
        debugprint(Log::DebugID::ICL_SetAssociative,
                   "READ   | REQ %7u | Cache miss (%u, %u) | %" PRIu64
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

        line->tag = iter->req.address;
        line->valid = true;

        debugprint(Log::DebugID::ICL_SetAssociative,
                   "READ   | REQ %7u | Cache hit delayed (%u, %u) | %" PRIu64
                   " - %" PRIu64 "(%" PRIu64 ")",
                   iter->req.id, iter->setIdx, iter->wayIdx, iter->submittedAt,
                   iter->finishedAt, iter->finishedAt - iter->submittedAt);

        // Add DRAM -> PCIe DMA latency
        object.dram->read(
            dataAddress + (iter->setIdx * waySize + iter->wayIdx) * lineSize,
            lineSize, eventReadDMADone, iter->id);

        readDMAQueue.emplace_back(*iter);
      }
      else if (iter->status == LineStatus::WriteHitReadPending) {
        iter->status = LineStatus::WriteCache;

        line->tag = iter->req.address;
        line->valid = true;
        line->dirty = true;

        scheduleNow(eventWriteMetaDone, iter->id);

        writeMetaQueue.emplace_back(*iter);
      }
      else if (iter->status == LineStatus::Invalidate) {
        // This line is invalidated by TRIM or Fomrat
      }
      else {
        panic("Unexpected line status.");
      }

      if (evictPolicy == Config::EvictModeType::LRU) {
        // Update clock at access
        line->clock = clock;
      }

      iter = readPendingQueue.erase(iter);
    }
    else {
      ++iter;
    }
  }
}

void SetAssociative::read_done(uint64_t tag) {
  auto ctx = findRequest(readDMAQueue, tag);

  scheduleNow(ctx.req.eid, ctx.req.data);
}

void SetAssociative::write_find(Request &&req) {
  CPU::Function fstat = CPU::initFunction();
  CacheContext ctx(req);
  ctx.id = requestCounter++;
  ctx.submittedAt = getTick();

  stat.request[1]++;

  debugprint(Log::DebugID::ICL_SetAssociative,
             "WRITE  | REQ %7u | LPN %" PRIx64 "h | SIZE %" PRIu64, ctx.req.id,
             ctx.req.address, lineSize - ctx.req.skipFront - ctx.req.skipEnd);

  // Update this if statement to support FUA
  if (LIKELY(writeEnabled)) {
    ctx.setIdx = getSetIndex(ctx.req.address);

    if (getValidWay(ctx.req.address, ctx.setIdx, ctx.wayIdx)) {
      // Hit (Update line)
      auto line = getLine(ctx.setIdx, ctx.wayIdx);

      stat.cache[1]++;

      if (line->rpending) {
        // Request is read pending
        ctx.status = LineStatus::WriteHitReadPending;
      }
      else if (line->wpending) {
        // Request is write pending
        ctx.status = LineStatus::WriteHitWritePending;
      }
      else {
        line->tag = ctx.req.address;
        line->valid = true;
        line->dirty = true;

        if (evictPolicy == Config::EvictModeType::LRU) {
          // Update clock at access
          line->clock = clock;
        }

        ctx.status = LineStatus::WriteCache;
      }
    }
    else if (getEmptyWay(ctx.setIdx, ctx.wayIdx)) {  // Cold miss
      auto line = getLine(ctx.setIdx, ctx.wayIdx);

      stat.cache[1]++;

      line->tag = ctx.req.address;
      line->clock = clock;
      line->valid = true;
      line->dirty = false;
      line->rpending = false;
      line->wpending = false;

      ctx.status = LineStatus::WriteCache;
    }
    else {  // Conflict miss
      ctx.status = LineStatus::WriteEvict;
    }

    scheduleFunction(CPU::CPUGroup::InternalCache, eventWritePreCPUDone, ctx.id,
                     fstat);
  }
  else {
    ctx.status = LineStatus::WriteNVM;

    // Set arbitraray set ID to pervent DRAM write queue hit
    ctx.wayIdx = ctx.id % waySize;
    ctx.setIdx = (ctx.id / waySize) % setSize;

    scheduleNow(eventWriteMetaDone, ctx.id);
  }

  writeMetaQueue.emplace_back(ctx);
}

void SetAssociative::write_findDone(uint64_t tag) {
  auto ctx = findRequest(writeMetaQueue, tag);

  // Add metadata access latency (for one set - all way lines)
  object.sram->read(metaAddress + ctx.setIdx * waySize * metaLineSize,
                    metaLineSize * waySize, eventWriteMetaDone, tag);

  writeMetaQueue.emplace_back(ctx);
}

void SetAssociative::write_dodram(uint64_t tag) {
  auto ctx = findRequest(writeMetaQueue, tag);

  switch (ctx.status) {
    case LineStatus::WriteHitReadPending:
      scheduleNow(eventWriteDRAMDone, tag);

      readPendingQueue.emplace_back(ctx);

      return;
    case LineStatus::WriteHitWritePending:
      scheduleNow(eventWriteDRAMDone, tag);

      writePendingQueue.emplace_back(ctx);

      return;
    case LineStatus::WriteCache:
      break;
    case LineStatus::WriteEvict:
    case LineStatus::WriteNVM:
      // Evict first
      evictQueue.emplace_back(ctx);

      evict(ctx.setIdx, ctx.status == LineStatus::WriteNVM);

      return;
    default:
      panic("Unexpected line status.");

      break;
  }

  // PCIe -> DRAM latency
  object.dram->write(
      dataAddress + (ctx.setIdx * waySize + ctx.wayIdx) * lineSize, lineSize,
      eventWriteDRAMDone, tag);

  writeDRAMQueue.emplace_back(ctx);
}

void SetAssociative::write_done(uint64_t now, uint64_t tag) {
  auto ctx = findRequest(writeDRAMQueue, tag);

  ctx.finishedAt = now;

  debugprint(Log::DebugID::ICL_SetAssociative,
             "WRITE  | REQ %7u | Cache hit (%u, %u) | %" PRIu64 " - %" PRIu64
             "(%" PRIu64 ")",
             ctx.req.id, ctx.setIdx, ctx.wayIdx, ctx.submittedAt,
             ctx.finishedAt, ctx.finishedAt - ctx.submittedAt);

  scheduleNow(ctx.req.eid, ctx.req.data);
}

void SetAssociative::prefetch(LPN begin, LPN end) {
  CPU::Function fstat = CPU::initFunction();
  uint32_t setIdx;
  uint32_t wayIdx;

  // Check which page to read
  for (LPN i = begin; i < end; i++) {
    setIdx = getSetIndex(i);

    if (getValidWay(i, setIdx, wayIdx)) {
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

void SetAssociative::evict(uint32_t set, bool fua) {
  if (UNLIKELY(fua)) {
    // set is invalid
    for (auto iter = evictQueue.begin(); iter != evictQueue.end();) {
      if (iter->status == LineStatus::WriteNVM) {
        evictFTLQueue.emplace_back(*iter);

        // DRAM -> NVM latency
        object.dram->read(
            dataAddress + (iter->setIdx * waySize + iter->wayIdx) * lineSize,
            lineSize, eventWriteDRAMDone, iter->id);

        iter = evictQueue.erase(iter);
      }
      else {
        ++iter;
      }
    }
  }
  else {
    uint32_t setIdx;
    uint32_t wayIdx;

    // We need to make empty way at set
    // 1. Select victim way
    wayIdx = chooseLine(set);

    // 2. Make page list to create # evictPages count of pages
    LPN begin = (getLine(set, wayIdx)->tag / evictPages) * evictPages;
    LPN end = begin + evictPages;

    // 3. Collect pages
    for (LPN i = begin; i < end; i++) {
      setIdx = getSetIndex(i);

      if (getValidWay(i, setIdx, wayIdx)) {
        // We have valid data
        auto line = getLine(setIdx, wayIdx);

        if (line->dirty) {
          // We need to evict this data
          markEvict(line->tag, setIdx, wayIdx);
        }
        else {
          // Clear line
          line->valid = false;
          line->dirty = false;
          line->rpending = false;
          line->wpending = false;
        }
      }
      else if (getEmptyWay(setIdx, wayIdx)) {
        // Empty way exists, skip
      }
      else {
        // No empty way, evict this set
        wayIdx = chooseLine(setIdx);

        auto line = getLine(setIdx, wayIdx);

        markEvict(line->tag, setIdx, wayIdx);
      }
    }

    // 4. Evict begin
    for (auto &iter : evictFTLQueue) {
      // DRAM -> NVM latency
      object.dram->read(
          dataAddress + (iter.setIdx * waySize + iter.wayIdx) * lineSize,
          lineSize, eventWriteDRAMDone, iter.id);
    }
  }
}

void SetAssociative::markEvict(LPN i, uint32_t setIdx, uint32_t wayIdx) {
  // Make request
  CacheContext ctx;

  ctx.id = requestCounter++;
  ctx.req.address = i;
  ctx.setIdx = setIdx;
  ctx.wayIdx = wayIdx;
  ctx.status = LineStatus::Eviction;
  ctx.submittedAt = getTick();

  evictFTLQueue.emplace_back(ctx);
}

void SetAssociative::evict_doftl(uint64_t tag) {
  auto ctx = findRequest(evictFTLQueue, tag);

  // Perform write
  pFTL->submit(FTL::Request(ctx.req.id, eventEvictFTLDone, ctx.id,
                            FTL::Operation::Write, ctx.req.address,
                            ctx.req.buffer));

  evictFTLQueue.emplace_back(ctx);
}

void SetAssociative::evict_done(uint64_t now, uint64_t tag) {
  auto ctx = findRequest(evictFTLQueue, tag);

  // Write done
  ctx.finishedAt = now;

  if (ctx.status == LineStatus::Eviction) {
    auto line = getLine(ctx.setIdx, ctx.wayIdx);

    line->wpending = false;

    // We will not transfer this request to host (complete here)
    debugprint(Log::DebugID::ICL_SetAssociative,
               "WRITE  | EVICTION    | Cache (%u, %u) | %" PRIu64 " - %" PRIu64
               "(%" PRIu64 ")",
               ctx.setIdx, ctx.wayIdx, ctx.submittedAt, ctx.finishedAt,
               ctx.finishedAt - ctx.submittedAt);

    if (UNLIKELY(flushQueue.size() > 0)) {
      // Find which flush request
      for (auto iter = flushQueue.begin(); iter != flushQueue.end(); ++iter) {
        if (ctx.req.address >= iter->req.address &&
            ctx.req.address < iter->req.address + iter->req.length) {
          iter->finishedAt--;

          if (iter->finishedAt == 0) {
            // Complete flush request if every pages are evicted
            debugprint(
                Log::DebugID::ICL_SetAssociative,
                "FLUSH  | REQ %7u | %" PRIu64 " - %" PRIu64 "(%" PRIu64 ")",
                iter->req.id, iter->submittedAt, now, now - iter->submittedAt);

            scheduleNow(iter->req.eid, iter->req.data);

            flushQueue.erase(iter);
          }

          break;
        }
      }
    }
  }
  else if (ctx.status == LineStatus::WriteNVM) {
    debugprint(Log::DebugID::ICL_SetAssociative,
               "WRITE | REQ %7u | FUA | %" PRIu64 " - %" PRIu64 "(%" PRIu64 ")",
               ctx.req.id, ctx.submittedAt, ctx.finishedAt,
               ctx.finishedAt - ctx.submittedAt);

    scheduleNow(ctx.req.eid, ctx.req.data);

    return;
  }
  else {
    panic("Unexpected line status.");
  }

  // At this point, we can handle pending request
  for (auto iter = evictQueue.begin(); iter != evictQueue.end();) {
    auto line = getLine(iter->setIdx, iter->wayIdx);

    if (line->wpending == false) {
      // Now this request can complete
      if (iter->status == LineStatus::WriteHitWritePending) {
        iter->status = LineStatus::WriteCache;

        line->tag = iter->req.address;
        line->valid = true;
        line->dirty = true;

        if (evictPolicy == Config::EvictModeType::LRU) {
          // Update clock at access
          line->clock = clock;
        }

        scheduleNow(eventWriteMetaDone, iter->id);

        writeMetaQueue.emplace_back(*iter);
      }
      else if (iter->status == LineStatus::Invalidate) {
        // This line is invalidated by TRIM or Fomrat
      }
      else {
        panic("Unexpected line status.");
      }

      iter = evictQueue.erase(iter);
    }
    else {
      ++iter;
    }
  }
}

void SetAssociative::flush_find(Request &&req) {
  CPU::Function fstat = CPU::initFunction();
  CacheContext ctx(req);
  ctx.id = requestCounter++;
  ctx.submittedAt = getTick();

  debugprint(Log::DebugID::ICL_SetAssociative,
             "FLUSH  | REQ %7u | LPN %" PRIx64 "h | SIZE %" PRIu64, ctx.req.id,
             ctx.req.address, lineSize - ctx.req.skipFront - ctx.req.skipEnd);

  if (LIKELY(writeEnabled)) {
    uint32_t setIdx;
    uint32_t wayIdx;
    bool found = false;

    for (LPN i = ctx.req.address; i < ctx.req.address + ctx.req.length; i++) {
      setIdx = getSetIndex(i);

      if (getValidWay(i, setIdx, wayIdx)) {
        auto line = getLine(setIdx, wayIdx);

        if (line->dirty && !line->wpending) {
          line->wpending = true;
          line->dirty = false;

          markEvict(i, setIdx, wayIdx);

          found = true;
          ctx.finishedAt++;  // Use this variable as total pages to flush
        }
      }
    }

    ctx.status = found ? LineStatus::Flush : LineStatus::FlushNone;

    scheduleFunction(CPU::CPUGroup::InternalCache, eventFlushPreCPUDone, ctx.id,
                     fstat);
  }
  else {
    // Nothing to flush - no dirty lines!
    ctx.status = LineStatus::FlushNone;

    scheduleNow(eventFlushMetaDone, ctx.id);
  }

  flushMetaQueue.emplace_back(ctx);
}

void SetAssociative::flush_findDone(uint64_t tag) {
  auto ctx = findRequest(flushMetaQueue, tag);

  // Add metadata access latency (for all set/way)
  object.sram->read(metaAddress, metaLineSize * setSize * waySize,
                    eventFlushMetaDone, tag);

  flushMetaQueue.emplace_back(ctx);
}

void SetAssociative::flush_doevict(uint64_t tag) {
  auto ctx = findRequest(flushMetaQueue, tag);

  switch (ctx.status) {
    case LineStatus::Flush:
      // flush requests in evictQueue
      evict(0, true);

      flushQueue.emplace_back(ctx);

      return;
    case LineStatus::FlushNone:
      // Completion
      scheduleNow(ctx.req.eid, ctx.req.data);

      return;
    default:
      panic("Unexpected line status.");

      break;
  }
}

void SetAssociative::invalidate_find(Request &&req) {
  CPU::Function fstat = CPU::initFunction();
  CacheContext ctx(req);
  ctx.id = requestCounter++;
  ctx.submittedAt = getTick();

  debugprint(Log::DebugID::ICL_SetAssociative,
             "%s | REQ %7u | LPN %" PRIx64 "h | SIZE %" PRIu64,
             ctx.req.opcode == Operation::Trim ? "TRIM  " : "FORMAT",
             ctx.req.id, ctx.req.address,
             lineSize - ctx.req.skipFront - ctx.req.skipEnd);

  if (LIKELY(writeEnabled)) {
    uint32_t setIdx;
    uint32_t wayIdx;

    for (LPN i = ctx.req.address; i < ctx.req.address + ctx.req.length; i++) {
      setIdx = getSetIndex(i);

      if (getValidWay(i, setIdx, wayIdx)) {
        auto line = getLine(setIdx, wayIdx);

        // Invalidate line
        line->valid = false;
        line->dirty = false;

        if (line->rpending) {
          CacheContext ctx2;

          ctx2.id = requestCounter++;
          ctx2.req.address = i;
          ctx2.setIdx = setIdx;
          ctx2.wayIdx = wayIdx;
          ctx2.status = LineStatus::Invalidate;
          ctx2.submittedAt = getTick();

          readPendingQueue.emplace_back(ctx2);
        }
        else if (line->wpending) {
          CacheContext ctx2;

          ctx2.id = requestCounter++;
          ctx2.req.address = i;
          ctx2.setIdx = setIdx;
          ctx2.wayIdx = wayIdx;
          ctx2.status = LineStatus::Invalidate;
          ctx2.submittedAt = getTick();

          writePendingQueue.emplace_back(ctx2);
        }
        else {
          line->rpending = false;
          line->wpending = false;
        }
      }
    }

    scheduleFunction(CPU::CPUGroup::InternalCache, eventInvalidatePreCPUDone,
                     ctx.id, fstat);
  }
  else {
    scheduleNow(eventInvalidateMetaDone, ctx.id);
  }

  ctx.status = LineStatus::Invalidate;

  invalidateMetaQueue.emplace_back(ctx);
}

void SetAssociative::invalidate_findDone(uint64_t tag) {
  auto ctx = findRequest(invalidateMetaQueue, tag);

  // Add metadata access latency (for all set/way)
  object.sram->read(metaAddress, metaLineSize * setSize * waySize,
                    eventFlushMetaDone, tag);

  invalidateMetaQueue.emplace_back(ctx);
}

void SetAssociative::invalidate_doftl(uint64_t tag) {
  auto ctx = findRequest(invalidateMetaQueue, tag);
  auto opcode = ctx.req.opcode == Operation::Trim ? FTL::Operation::Trim
                                                  : FTL::Operation::Format;

  // Perform invalidate
  pFTL->submit(FTL::Request(ctx.req.id, eventInvalidateFTLDone, ctx.id, opcode,
                            ctx.req.address, ctx.req.length));

  invalidateFTLQueue.emplace_back(ctx);
}

void SetAssociative::invalidate_done(uint64_t now, uint64_t tag) {
  auto ctx = findRequest(invalidateFTLQueue, tag);

  debugprint(Log::DebugID::ICL_SetAssociative,
             "%s | REQ %7u | %" PRIu64 " - %" PRIu64 "(%" PRIu64 ")",
             ctx.req.opcode == Operation::Trim ? "TRIM  " : "FORMAT",
             ctx.req.id, ctx.submittedAt, now, now - ctx.submittedAt);

  scheduleNow(ctx.req.eid, ctx.req.data);
}

void SetAssociative::backupQueue(std::ostream &out,
                                 const std::list<CacheContext> *queue) const {
  uint64_t size = queue->size();
  BACKUP_SCALAR(out, size);

  for (auto &iter : *queue) {
    iter.req.backup(out);

    BACKUP_SCALAR(out, iter.id);
    BACKUP_SCALAR(out, iter.setIdx);
    BACKUP_SCALAR(out, iter.wayIdx);
    BACKUP_SCALAR(out, iter.submittedAt);
    BACKUP_SCALAR(out, iter.finishedAt);
    BACKUP_SCALAR(out, iter.status);
  }
}

void SetAssociative::restoreQueue(std::istream &in,
                                  std::list<CacheContext> *queue) {
  uint64_t size;

  RESTORE_SCALAR(in, size);

  for (uint64_t i = 0; i < size; i++) {
    CacheContext ctx;

    ctx.req.restore(object, in);

    RESTORE_SCALAR(in, ctx.id);
    RESTORE_SCALAR(in, ctx.setIdx);
    RESTORE_SCALAR(in, ctx.wayIdx);
    RESTORE_SCALAR(in, ctx.submittedAt);
    RESTORE_SCALAR(in, ctx.finishedAt);
    RESTORE_SCALAR(in, ctx.status);

    queue->emplace_back(ctx);
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

void SetAssociative::createCheckpoint(std::ostream &out) const noexcept {
  BACKUP_SCALAR(out, lineSize);
  BACKUP_SCALAR(out, setSize);
  BACKUP_SCALAR(out, waySize);
  BACKUP_BLOB(out, cacheMetadata, sizeof(Line) * setSize * waySize);
  BACKUP_SCALAR(out, readEnabled);
  BACKUP_SCALAR(out, writeEnabled);
  BACKUP_SCALAR(out, prefetchEnabled);
  BACKUP_SCALAR(out, requestCounter);
  BACKUP_SCALAR(out, trigger.lastRequestID);
  BACKUP_SCALAR(out, trigger.requestCounter);
  BACKUP_SCALAR(out, trigger.requestCapacity);
  BACKUP_SCALAR(out, trigger.lastAddress);
  BACKUP_SCALAR(out, prefetchPages);
  BACKUP_SCALAR(out, evictPages);
  BACKUP_SCALAR(out, metaAddress);
  BACKUP_SCALAR(out, metaLineSize);
  BACKUP_SCALAR(out, dataAddress);
  BACKUP_SCALAR(out, clock);
  BACKUP_SCALAR(out, evictPolicy);
  BACKUP_BLOB(out, &stat, sizeof(stat));

  backupQueue(out, &readPendingQueue);
  backupQueue(out, &readMetaQueue);
  backupQueue(out, &readFTLQueue);
  backupQueue(out, &readDRAMQueue);
  backupQueue(out, &readDMAQueue);
  backupQueue(out, &writePendingQueue);
  backupQueue(out, &writeMetaQueue);
  backupQueue(out, &writeDRAMQueue);
  backupQueue(out, &evictQueue);
  backupQueue(out, &evictFTLQueue);
  backupQueue(out, &flushMetaQueue);
  backupQueue(out, &flushQueue);
  backupQueue(out, &invalidateMetaQueue);
  backupQueue(out, &invalidateFTLQueue);

  BACKUP_EVENT(out, eventReadPreCPUDone);
  BACKUP_EVENT(out, eventReadMetaDone);
  BACKUP_EVENT(out, eventReadFTLDone);
  BACKUP_EVENT(out, eventReadDRAMDone);
  BACKUP_EVENT(out, eventReadDMADone);
  BACKUP_EVENT(out, eventWritePreCPUDone);
  BACKUP_EVENT(out, eventWriteMetaDone);
  BACKUP_EVENT(out, eventWriteDRAMDone);
  BACKUP_EVENT(out, eventEvictDRAMDone);
  BACKUP_EVENT(out, eventEvictFTLDone);
  BACKUP_EVENT(out, eventFlushPreCPUDone);
  BACKUP_EVENT(out, eventFlushMetaDone);
  BACKUP_EVENT(out, eventInvalidatePreCPUDone);
  BACKUP_EVENT(out, eventInvalidateMetaDone);
  BACKUP_EVENT(out, eventInvalidateFTLDone);
}

void SetAssociative::restoreCheckpoint(std::istream &in) noexcept {
  uint32_t tmp32;
  bool tmpb;

  RESTORE_SCALAR(in, tmp32);
  panic_if(tmp32 != lineSize, "Line size not matched while restore.");

  RESTORE_SCALAR(in, tmp32);
  panic_if(tmp32 != setSize, "Set size not matched while restore.");

  RESTORE_SCALAR(in, tmp32);
  panic_if(tmp32 != waySize, "Way size not matched while restore.");

  RESTORE_BLOB(in, cacheMetadata, sizeof(Line) * setSize * waySize);

  RESTORE_SCALAR(in, tmpb);
  panic_if(tmpb != readEnabled, "readEnabled not matched while restore.");

  RESTORE_SCALAR(in, tmpb);
  panic_if(tmpb != writeEnabled, "writeEnabled not matched while restore.");

  RESTORE_SCALAR(in, tmpb);
  panic_if(tmpb != prefetchEnabled,
           "prefetchEnabled not matched while restore.");

  RESTORE_SCALAR(in, requestCounter);
  RESTORE_SCALAR(in, trigger.lastRequestID);
  RESTORE_SCALAR(in, trigger.requestCounter);
  RESTORE_SCALAR(in, trigger.requestCapacity);
  RESTORE_SCALAR(in, trigger.lastAddress);
  RESTORE_SCALAR(in, prefetchPages);
  RESTORE_SCALAR(in, evictPages);
  RESTORE_SCALAR(in, metaAddress);
  RESTORE_SCALAR(in, metaLineSize);
  RESTORE_SCALAR(in, dataAddress);
  RESTORE_SCALAR(in, clock);
  RESTORE_SCALAR(in, evictPolicy);
  RESTORE_BLOB(in, &stat, sizeof(stat));

  restoreQueue(in, &readPendingQueue);
  restoreQueue(in, &readMetaQueue);
  restoreQueue(in, &readFTLQueue);
  restoreQueue(in, &readDRAMQueue);
  restoreQueue(in, &readDMAQueue);
  restoreQueue(in, &writePendingQueue);
  restoreQueue(in, &writeMetaQueue);
  restoreQueue(in, &writeDRAMQueue);
  restoreQueue(in, &evictQueue);
  restoreQueue(in, &evictFTLQueue);
  restoreQueue(in, &flushMetaQueue);
  restoreQueue(in, &flushQueue);
  restoreQueue(in, &invalidateMetaQueue);
  restoreQueue(in, &invalidateFTLQueue);

  RESTORE_EVENT(in, eventReadPreCPUDone);
  RESTORE_EVENT(in, eventReadMetaDone);
  RESTORE_EVENT(in, eventReadFTLDone);
  RESTORE_EVENT(in, eventReadDRAMDone);
  RESTORE_EVENT(in, eventReadDMADone);
  RESTORE_EVENT(in, eventWritePreCPUDone);
  RESTORE_EVENT(in, eventWriteMetaDone);
  RESTORE_EVENT(in, eventWriteDRAMDone);
  RESTORE_EVENT(in, eventEvictDRAMDone);
  RESTORE_EVENT(in, eventEvictFTLDone);
  RESTORE_EVENT(in, eventFlushPreCPUDone);
  RESTORE_EVENT(in, eventFlushMetaDone);
  RESTORE_EVENT(in, eventInvalidatePreCPUDone);
  RESTORE_EVENT(in, eventInvalidateMetaDone);
  RESTORE_EVENT(in, eventInvalidateFTLDone);
}

}  // namespace SimpleSSD::ICL
