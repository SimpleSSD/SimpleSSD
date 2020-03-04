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

  cacheDataSize = parameter->pageSize;

  sectorsInCacheLine = cacheDataSize / minIO;

  // Allocate cachelines
  setSize = 0;
  waySize = (uint32_t)readConfigUint(Section::InternalCache,
                                     Config::Key::CacheWaySize);

  uint64_t cacheSize =
      readConfigUint(Section::InternalCache, Config::Key::CacheSize);

  if (waySize == 0) {
    // Fully-associative
    setSize = 1;
    waySize = MAX(cacheSize / cacheDataSize, 1);
  }
  else {
    setSize = MAX(cacheSize / cacheDataSize / waySize, 1);
  }

  cacheline.reserve(setSize * waySize);

  for (uint64_t i = 0; i < (uint64_t)setSize * waySize; i++) {
    cacheline.emplace_back(CacheLine(sectorsInCacheLine));
  }

  cacheSize = (uint64_t)setSize * waySize * cacheDataSize;

  debugprint(
      Log::DebugID::ICL_SetAssociative,
      "CREATE  | Set size %u | Way size %u | Line size %u | Capacity %" PRIu64,
      setSize, waySize, cacheDataSize, cacheSize);

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

  // Create events
  eventLookupMemory =
      createEvent([this](uint64_t, uint64_t d) { readSet(d, eventLookupDone); },
                  "ICL::SetAssociative::eventLookupMemory");
  eventLookupDone =
      createEvent([this](uint64_t, uint64_t d) { manager->lookupDone(d); },
                  "ICL::SetAssociative::eventLookupDone");
  eventReadTag =
      createEvent([this](uint64_t, uint64_t d) { readAll(d, eventCacheDone); },
                  "ICL::SetAssociative::eventReadTag");
  eventCacheDone =
      createEvent([this](uint64_t, uint64_t d) { manager->cacheDone(d); },
                  "ICL::SetAssociative::eventCacheDone");
}

SetAssociative::~SetAssociative() {}

CPU::Function SetAssociative::randomEviction(uint32_t set, uint32_t &way) {
  CPU::Function fstat;
  CPU::markFunction(fstat);

  // Check all lines are pending
  bool skip = true;

  for (way = 0; way < waySize; way++) {
    auto &line = cacheline[set * waySize + way];

    if (!line.dmaPending && !line.nvmPending) {
      skip = false;

      break;
    }
  }

  if (!skip) {
    do {
      way = dist(gen);

      auto &line = cacheline[set * waySize + way];

      if (line.valid) {
        break;
      }
    } while (true);
  }

  return fstat;
}

CPU::Function SetAssociative::fifoEviction(uint32_t set, uint32_t &way) {
  CPU::Function fstat;
  CPU::markFunction(fstat);

  way = waySize;

  uint64_t min = std::numeric_limits<uint64_t>::max();

  for (uint32_t i = 0; i < waySize; i++) {
    auto &line = cacheline[set * waySize + i];

    if (line.insertedAt < min && !line.dmaPending && !line.nvmPending) {
      min = line.insertedAt;
      way = i;
    }
  }

  return fstat;
}

CPU::Function SetAssociative::lruEviction(uint32_t set, uint32_t &way) {
  CPU::Function fstat;
  CPU::markFunction(fstat);

  way = waySize;

  uint64_t min = std::numeric_limits<uint64_t>::max();

  for (uint32_t i = 0; i < waySize; i++) {
    auto &line = cacheline[set * waySize + i];

    if (line.accessedAt < min && !line.dmaPending && !line.nvmPending) {
      min = line.accessedAt;
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

void SetAssociative::readAll(uint64_t tag, Event eid) {
  object.memory->read(cacheTagBaseAddress, cacheTagSize * setSize * waySize,
                      eid, tag);
}

void SetAssociative::readSet(uint64_t tag, Event eid) {
  auto req = getSubRequest(tag);
  auto set = getSetIdx(req->getLPN());

  object.memory->read(makeTagAddress(set), cacheTagSize * waySize, eid, tag);
}

void SetAssociative::lookup(HIL::SubRequest *sreq) {
  CPU::Function fstat;
  CPU::markFunction(fstat);

  LPN lpn = sreq->getLPN();
  uint32_t set = getSetIdx(lpn);
  uint32_t way;

  fstat += getValidWay(lpn, way);

  if (way == waySize) {
    // Miss, we need allocation
    sreq->setAllocate();
  }

  object.memory->read(cacheTagBaseAddress, cacheTagSize * waySize,
                      InvalidEventID);

  scheduleFunction(CPU::CPUGroup::InternalCache, eventLookupMemory,
                   sreq->getTag(), fstat);
}

void SetAssociative::flush(HIL::SubRequest *sreq) {
  CPU::Function fstat;
  CPU::markFunction(fstat);

  std::vector<FlushContext> list;

  LPN slpn = sreq->getOffset();
  uint32_t nlp = sreq->getLength();
  uint64_t i = 0;

  for (auto &iter : cacheline) {
    if (iter.valid && !iter.nvmPending && !iter.dmaPending &&
        slpn <= iter.tag && iter.tag < slpn + nlp) {
      iter.nvmPending = true;

      list.emplace_back(iter.tag, cacheDataBaseAddress + cacheDataSize * i);
    }

    i++;
  }

  flushList.emplace_back(sreq->getTag(), slpn, nlp, (uint32_t)list.size());

  manager->drain(list);

  scheduleFunction(CPU::CPUGroup::InternalCache, eventReadTag, sreq->getTag(),
                   fstat);
}

void SetAssociative::erase(HIL::SubRequest *sreq) {
  CPU::Function fstat;
  CPU::markFunction(fstat);

  LPN slpn = sreq->getOffset();
  uint32_t nlp = sreq->getLength();

  for (auto &iter : cacheline) {
    if (iter.valid && slpn <= iter.tag && iter.tag < slpn + nlp) {
      iter.valid = false;
    }
  }

  scheduleFunction(CPU::CPUGroup::InternalCache, eventReadTag, sreq->getTag(),
                   fstat);
}

void SetAssociative::allocate(HIL::SubRequest *sreq) {
  CPU::Function fstat;
  CPU::markFunction(fstat);
}

void SetAssociative::dmaDone(LPN lpn) {
  LPN lpn = sreq->getLPN();
  uint32_t set = getSetIdx(lpn);
  uint32_t way;

  getValidWay(lpn, way);

  if (way < waySize) {
    cacheline.at(set * waySize + way).dmaPending = false;
  }
}

void SetAssociative::nvmDone(LPN lpn) {
  bool found = false;

  for (auto iter = flushList.begin(); iter != flushList.end(); iter++) {
    auto fr = iter->lpnList.find(lpn);

    if (fr != iter->lpnList.end()) {
      auto &line = cacheline.at(fr->second.set * waySize + fr->second.way);

      // Clear all
      line.data = 0;

      // Erase
      iter->lpnList.erase(fr);

      if (iter->lpnList.size() == 0) {
        manager->cacheDone(iter->tag);

        flushList.erase(iter);
      }

      found = true;

      break;
    }
  }

  if (!found) {
    auto iter = evictList.find(lpn);

    panic_if(iter == evictList.end(), "Unexpected completion.");

    auto &line = cacheline.at(iter->second.set * waySize + iter->second.way);

    line.nvmPending = false;

    evictList.erase(iter);

    // TODO: Check allocation space
  }
}

void SetAssociative::getStatList(std::vector<Stat> &, std::string) noexcept {}

void SetAssociative::getStatValues(std::vector<double> &) noexcept {}

void SetAssociative::resetStatValues() noexcept {}

void SetAssociative::createCheckpoint(std::ostream &) const noexcept {}

void SetAssociative::restoreCheckpoint(std::istream &) noexcept {}

}  // namespace SimpleSSD::ICL
