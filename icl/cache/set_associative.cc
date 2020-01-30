// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "icl/cache/set_associative.hh"

#include "icl/manager/abstract_manager.hh"
#include "util/algorithm.hh"

namespace SimpleSSD::ICL {

SetAssociative::SetAssociative(ObjectData &o, AbstractManager *m,
                               FTL::Parameter *p)
    : AbstractCache(o, m, p), gen(rd()) {
  evictMode = (Config::Granularity)readConfigUint(
      Section::InternalCache, Config::Key::EvictGranularity);

  auto policy = (Config::EvictPolicyType)readConfigUint(
      Section::InternalCache, Config::Key::EvictPolicy);

  uint32_t lineSize = parameter->pageSize;

  sectorsInCacheLine = lineSize / minIO;

  // Allocate cachelines
  setSize = 0;
  waySize = (uint32_t)readConfigUint(Section::InternalCache,
                                     Config::Key::CacheWaySize);

  uint64_t cacheSize =
      readConfigUint(Section::InternalCache, Config::Key::CacheSize);

  if (waySize == 0) {
    // Fully-associative
    setSize = 1;
    waySize = MAX(cacheSize / lineSize, 1);
  }
  else {
    setSize = MAX(cacheSize / lineSize / waySize, 1);
  }

  cacheline.reserve(setSize * waySize);

  for (uint64_t i = 0; i < (uint64_t)setSize * waySize; i++) {
    cacheline.emplace_back(CacheLine(sectorsInCacheLine));
  }

  cacheSize = (uint64_t)setSize * waySize * lineSize;

  debugprint(
      Log::DebugID::ICL_SetAssociative,
      "CREATE  | Set size %u | Way size %u | Line size %u | Capacity %" PRIu64,
      setSize, waySize, lineSize, cacheSize);

  // # pages to evict once
  switch (evictMode) {
    case Config::Granularity::SuperpageLevel:
      pagesToEvict = parameter->parallelismLevel[0];

      break;
    case Config::Granularity::AllLevel:
      pagesToEvict = parameter->parallelism;

      break;
    default:
      panic("Unexpected eviction granularity.");

      break;
  }

  // Allocate memory
  cacheDataBaseAddress = object.memory->allocate(
      cacheSize, Memory::MemoryType::DRAM, "ICL::SetAssociative::Data");

  // Omit insertedAt and accessedAt
  cacheTagSize = 8 + DIVCEIL(sectorsInCacheLine, 8);

  uint64_t totalTagSize = cacheTagSize * setSize * waySize;

  // Try SRAM
  if (object.memory->allocate(cacheTagSize, Memory::MemoryType::SRAM, "",
                              true) == 0) {
    cacheTagBaseAddress = object.memory->allocate(
        totalTagSize, Memory::MemoryType::SRAM, "ICL::SetAssociative::Tag");
  }
  else {
    cacheTagBaseAddress = object.memory->allocate(
        totalTagSize, Memory::MemoryType::DRAM, "ICL::SetAssociative::Tag");
  }

  // Create victim cacheline selecttion
  switch (policy) {
    case Config::EvictPolicyType::Random:
      dist = std::uniform_int_distribution<uint32_t>(0, waySize - 1);

      evictFunction = [this](uint32_t s, uint32_t &w) -> CPU::Function {
        return randomEviction(s, w);
      };

      break;
    case Config::EvictPolicyType::FIFO:
      evictFunction = [this](uint32_t s, uint32_t &w) -> CPU::Function {
        return fifoEviction(s, w);
      };

      break;
    case Config::EvictPolicyType::LRU:
      evictFunction = [this](uint32_t s, uint32_t &w) -> CPU::Function {
        return lruEviction(s, w);
      };

      break;
    default:
      panic("Unexpected eviction policy.");

      break;
  }
}

SetAssociative::~SetAssociative() {}

CPU::Function SetAssociative::randomEviction(uint32_t set, uint32_t &way) {
  CPU::Function fstat;
  CPU::markFunction(fstat);

  do {
    way = dist(gen);

    auto &line = cacheline[set * waySize + way];

    if (line.valid) {
      break;
    }
  } while (true);

  return fstat;
}

CPU::Function SetAssociative::fifoEviction(uint32_t set, uint32_t &way) {
  CPU::Function fstat;
  CPU::markFunction(fstat);

  uint64_t min = std::numeric_limits<uint64_t>::max();

  for (uint32_t i = 0; i < waySize; i++) {
    if (cacheline[set * waySize + i].insertedAt < min) {
      min = cacheline[set * waySize + i].insertedAt;
      way = i;
    }
  }

  return fstat;
}

CPU::Function SetAssociative::lruEviction(uint32_t set, uint32_t &way) {
  CPU::Function fstat;
  CPU::markFunction(fstat);

  uint64_t min = std::numeric_limits<uint64_t>::max();

  for (uint32_t i = 0; i < waySize; i++) {
    if (cacheline[set * waySize + i].accessedAt < min) {
      min = cacheline[set * waySize + i].accessedAt;
      way = i;
    }
  }

  return fstat;
}

CPU::Function SetAssociative::getEmptyWay(uint32_t set, uint32_t &way) {
  CPU::Function fstat;
  CPU::markFunction(fstat);

  for (way = 0; way < waySize; way++) {
    auto &line = cacheline[set * setSize + way];

    if (!line.valid) {
      break;
    }
  }

  return fstat;
}

CPU::Function SetAssociative::getValidWay(LPN lpn, uint32_t &way) {
  CPU::Function fstat;
  CPU::markFunction(fstat);

  uint32_t set = getSetIdx(lpn);

  for (way = 0; way < waySize; way++) {
    auto &line = cacheline[set * setSize + way];

    if (line.valid && line.tag == lpn) {
      break;
    }
  }

  return fstat;
}

CPU::Function SetAssociative::lookup(SubRequest *sreq, bool read) {
  CPU::Function fstat;
  CPU::markFunction(fstat);

  LPN lpn = sreq->getLPN();
  uint32_t set = getSetIdx(lpn);
  uint32_t way;

  fstat += getValidWay(lpn, way);

  if (way != waySize) {
    // Hit
    sreq->setHit();
  }
  else if (!read) {
    getEmptyWay(set, way);  // Don't add fstat here

    if (way != waySize) {
      // Cold miss
      sreq->setHit();
    }
  }

  object.memory->read(cacheTagBaseAddress, cacheTagSize * waySize,
                      InvalidEventID);

  return fstat;
}

CPU::Function SetAssociative::allocate(SubRequest *sreq) {
  CPU::Function fstat;
  CPU::markFunction(fstat);

  // Check capacity
  if (cacheline.size() < maxEntryCount) {
    // Just add new entry for current subrequest
    auto ret = cacheline.emplace(sreq->getLPN(), CacheLine(sectorsInCacheLine));

    panic_if(!ret.second, "Cache corrupted.");
  }
  else {
    // Select cachelines to evict
    uint32_t nl = parameter->parallelismLevel[0];
    std::vector<FlushContext> list;

    if (evictMode == Config::Granularity::AllLevel) {
      nl = parameter->parallelism;
    }

    for (uint32_t i = 0; i < nl; i++) {
      auto iter = evictFunction();

      panic_if(iter == cacheline.end(), "Cache corrupted.");

      list.emplace_back(FlushContext(iter->first, iter->second.address));
    }

    manager->drain(list);
  }

  return fstat;
}

CPU::Function SetAssociative::flush(SubRequest *sreq) {
  CPU::Function fstat;
  CPU::markFunction(fstat);

  return fstat;
}

CPU::Function SetAssociative::erase(SubRequest *) {}

void SetAssociative::dmaDone(LPN) {}

void SetAssociative::drainDone() {}

void SetAssociative::getStatList(std::vector<Stat> &, std::string) noexcept {}

void SetAssociative::getStatValues(std::vector<double> &) noexcept {}

void SetAssociative::resetStatValues() noexcept {}

void SetAssociative::createCheckpoint(std::ostream &) const noexcept {}

void SetAssociative::restoreCheckpoint(std::istream &) noexcept {}

}  // namespace SimpleSSD::ICL
