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
    : AbstractCache(o, m, p) {
  auto evictMode = (Config::Granularity)readConfigUint(
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
    case Config::EvictPolicyType::FIFO:
      evictFunction = [this](uint32_t s, uint32_t &w) -> CPU::Function {
        return fifoEviction(s, w);
      };
      compareFunction = [this](uint64_t a, uint64_t b) -> uint64_t {
        auto &ca = cacheline.at(a);
        auto &cb = cacheline.at(b);

        if (ca.insertedAt < cb.insertedAt) {
          return a;
        }

        return b;
      };

      break;
    case Config::EvictPolicyType::LRU:
      evictFunction = [this](uint32_t s, uint32_t &w) -> CPU::Function {
        return lruEviction(s, w);
      };
      compareFunction = [this](uint64_t a, uint64_t b) -> uint64_t {
        auto &ca = cacheline.at(a);
        auto &cb = cacheline.at(b);

        if (ca.accessedAt < cb.accessedAt) {
          return a;
        }

        return b;
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

CPU::Function SetAssociative::fifoEviction(uint32_t set, uint32_t &way) {
  CPU::Function fstat;
  CPU::markFunction(fstat);

  way = waySize;

  uint64_t min = std::numeric_limits<uint64_t>::max();

  for (uint32_t i = 0; i < waySize; i++) {
    auto &line = cacheline.at(set * waySize + i);

    if (line.valid && line.insertedAt < min && !line.dmaPending &&
        !line.nvmPending) {
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
    auto &line = cacheline.at(set * waySize + i);

    if (line.valid && line.accessedAt < min && !line.dmaPending &&
        !line.nvmPending) {
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
    auto &line = cacheline.at(set * waySize + way);

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
    auto &line = cacheline.at(set * waySize + way);

    if (line.valid && line.tag == lpn) {
      break;
    }
  }

  return fstat;
}

void SetAssociative::getCleanWay(uint32_t set, uint32_t &way) {
  way = waySize;

  for (uint32_t i = 0; i < waySize; i++) {
    auto &line = cacheline.at(set * waySize + i);

    if (line.valid && !line.dirty) {
      if (way == waySize) {
        way = i;
      }
      else {
        way = compareFunction(i, way);
      }
    }
  }
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

void SetAssociative::tryLookup(LPN lpn, bool flush) {
  auto iter = lookupList.find(lpn);

  if (iter != lookupList.end()) {
    if (flush) {
      // This was flush -> cacheline looked up was invalidated
      auto req = getSubRequest(iter->second);

      req->setAllocate();
    }

    manager->lookupDone(iter->second);
    lookupList.erase(iter);
  }
}

void SetAssociative::tryAllocate(LPN lpn) {
  auto iter = allocateList.find(getSetIdx(lpn));

  if (iter != allocateList.end()) {
    // Try allocate again
    auto req = getSubRequest(iter->second);

    // Must erase first
    allocateList.erase(iter);

    allocate(req);
  }
}

void SetAssociative::collect(uint32_t curSet, std::vector<FlushContext> &list) {
  uint64_t size = cacheline.size();
  std::vector<uint64_t> collected(pagesToEvict, size);

  for (uint64_t i = 0; i < size; i++) {
    auto &iter = cacheline.at(i);

    if (iter.valid && iter.dirty && !iter.dmaPending && !iter.nvmPending) {
      uint32_t offset = iter.tag % pagesToEvict;

      auto &line = collected.at(offset);

      if (line < size) {
        line = compareFunction(line, i);
      }
      else {
        line = i;
      }
    }
  }

  // Check curSet exists
  bool found = false;

  for (auto &i : collected) {
    if (i < size) {
      uint32_t set = i / waySize;

      if (set == curSet) {
        found = true;

        break;
      }
    }
  }

  // Make curSet always exists
  if (!found) {
    uint32_t way;

    evictFunction(curSet, way);

    if (way != waySize) {
      uint64_t i = curSet * waySize + way;
      auto &line = cacheline.at(i);
      uint32_t offset = line.tag % pagesToEvict;

      collected.at(offset) = i;
    }
  }

  // Prepare list
  list.reserve(collected.size());

  for (auto &i : collected) {
    if (i < size) {
      auto &line = cacheline.at(i);
      uint32_t set = i / waySize;
      uint32_t way = i % waySize;

      line.nvmPending = true;

      evictList.emplace(line.tag, LineInfo(set, way));
      list.emplace_back(FlushContext(line.tag, makeDataAddress(set, way)));
    }
  }
}

void SetAssociative::lookup(HIL::SubRequest *sreq) {
  CPU::Function fstat;
  CPU::markFunction(fstat);

  LPN lpn = sreq->getLPN();
  uint32_t set = getSetIdx(lpn);
  uint32_t way;

  fstat += getValidWay(lpn, way);

  if (way == waySize) {
    debugprint(Log::DebugID::ICL_SetAssociative,
               "LOOKUP | LPN %" PRIu64 " | Not found", lpn);

    // Miss, we need allocation
    sreq->setAllocate();
  }
  else {
    debugprint(Log::DebugID::ICL_SetAssociative,
               "LOOKUP | LPN %" PRIu64 " | (%u, %u)", lpn, set, way);

    sreq->setDRAMAddress(makeDataAddress(set, way));

    // Check NAND/DMA is pending
    auto &line = cacheline.at(set * waySize + way);
    auto opcode = sreq->getOpcode();

    if (line.dmaPending || line.nvmPending) {
      debugprint(Log::DebugID::ICL_SetAssociative,
                 "LOOKUP | LPN %" PRIu64 " | Pending", lpn, set, way);

      // We need to stall this lookup
      lookupList.emplace(line.tag, sreq->getTag());

      // TODO: This is not a solution
      return;
    }

    // Update
    line.accessedAt = getTick();

    if (opcode == HIL::Operation::Write ||
        opcode == HIL::Operation::WriteZeroes) {
      line.dirty = true;
    }
  }

  scheduleFunction(CPU::CPUGroup::InternalCache, eventLookupMemory,
                   sreq->getTag(), fstat);
}

void SetAssociative::flush(HIL::SubRequest *sreq) {
  CPU::Function fstat;
  CPU::markFunction(fstat);

  std::vector<FlushContext> list;
  std::unordered_map<LPN, LineInfo> lpnList;

  LPN slpn = sreq->getOffset();
  uint32_t nlp = sreq->getLength();
  uint64_t i = 0;

  debugprint(Log::DebugID::ICL_SetAssociative, "FLUSH  | LPN %" PRIu64 " + %u",
             slpn, nlp);

  for (auto &iter : cacheline) {
    if (iter.valid && !iter.nvmPending && !iter.dmaPending &&
        slpn <= iter.tag && iter.tag < slpn + nlp) {
      iter.nvmPending = true;

      auto ret =
          list.emplace_back(iter.tag, cacheDataBaseAddress + cacheDataSize * i);

      ret.offset = iter.validbits.ctz() * minIO;
      ret.length = cacheDataSize - iter.validbits.clz() * minIO - ret.offset;

      lpnList.emplace(iter.tag, LineInfo(i / waySize, i % waySize));
    }

    i++;
  }

  auto ret = flushList.emplace_back(sreq->getTag());

  ret.lpnList = std::move(lpnList);

  manager->drain(list);

  scheduleFunction(CPU::CPUGroup::InternalCache, eventReadTag, sreq->getTag(),
                   fstat);
}

void SetAssociative::erase(HIL::SubRequest *sreq) {
  CPU::Function fstat;
  CPU::markFunction(fstat);

  LPN slpn = sreq->getOffset();
  uint32_t nlp = sreq->getLength();

  debugprint(Log::DebugID::ICL_SetAssociative, "ERASE  | LPN %" PRIu64 " + %u",
             slpn, nlp);

  for (auto &iter : cacheline) {
    if (iter.valid && slpn <= iter.tag && iter.tag < slpn + nlp) {
      iter.data = 0;
      iter.validbits.reset();
    }
  }

  scheduleFunction(CPU::CPUGroup::InternalCache, eventReadTag, sreq->getTag(),
                   fstat);
}

void SetAssociative::allocate(HIL::SubRequest *sreq) {
  CPU::Function fstat;
  CPU::markFunction(fstat);

  LPN lpn = sreq->getLPN();
  uint32_t set = getSetIdx(lpn);
  uint32_t way;

  // Try allocate
  fstat += getEmptyWay(set, way);

  if (way == waySize) {
    // Double-check with clean line
    getCleanWay(set, way);
  }

  if (way == waySize) {
    debugprint(Log::DebugID::ICL_SetAssociative,
               "ALLOC  | LPN %" PRIu64 " | Pending", lpn);

    // Insert into pending queue
    allocateList.emplace(set, sreq->getTag());

    // Perform eviction
    std::vector<FlushContext> list;

    collect(set, list);

    manager->drain(list);

    // TODO: This is not a solution
    return;
  }
  else {
    debugprint(Log::DebugID::ICL_SetAssociative,
               "ALLOC  | LPN %" PRIu64 " | (%u, %u)", lpn, set, way);

    // Set DRAM address
    sreq->setDRAMAddress(makeDataAddress(set, way));

    // Fill cacheline
    auto &line = cacheline.at(set * waySize + way);

    line.valid = true;
    line.tag = lpn;
    line.insertedAt = getTick();
    line.accessedAt = getTick();

    // Partial update only if write
    auto opcode = sreq->getOpcode();

    if (opcode == HIL::Operation::Write ||
        opcode == HIL::Operation::WriteZeroes) {
      line.dirty = true;

      updateSkip(line.validbits, sreq);
    }
    else {
      line.nvmPending = true;  // Read is triggered immediately
      line.validbits.set();
    }
  }

  // No memory access because we already do that in lookup phase
  scheduleFunction(CPU::CPUGroup::InternalCache, eventCacheDone, sreq->getTag(),
                   fstat);
}

void SetAssociative::dmaDone(LPN lpn) {
  uint32_t set = getSetIdx(lpn);
  uint32_t way;

  getValidWay(lpn, way);

  if (way < waySize) {
    auto &line = cacheline.at(set * waySize + way);

    line.dmaPending = false;

    // Lookup
    tryLookup(lpn);

    // Allocate
    tryAllocate(lpn);
  }
}

void SetAssociative::nvmDone(LPN lpn) {
  bool found = false;

  // Flush
  for (auto iter = flushList.begin(); iter != flushList.end(); iter++) {
    auto fr = iter->lpnList.find(lpn);

    if (fr != iter->lpnList.end()) {
      auto &line = cacheline.at(fr->second.set * waySize + fr->second.way);

      // Not dirty now
      line.dirty = false;

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

  // Eviction
  if (!found) {
    auto iter = evictList.find(lpn);

    if (iter != evictList.end()) {
      auto &line = cacheline.at(iter->second.set * waySize + iter->second.way);

      line.dirty = false;
      line.nvmPending = false;

      evictList.erase(iter);
    }
  }

  // Read
  if (!found) {
    uint32_t set = getSetIdx(lpn);

    for (uint32_t way = 0; way < waySize; way++) {
      auto &line = cacheline.at(set * waySize + way);

      if (line.tag == lpn) {
        line.nvmPending = false;

        break;
      }
    }
  }

  // Lookup
  tryLookup(lpn, found);

  // Allocate
  tryAllocate(lpn);
}

void SetAssociative::getStatList(std::vector<Stat> &, std::string) noexcept {}

void SetAssociative::getStatValues(std::vector<double> &) noexcept {}

void SetAssociative::resetStatValues() noexcept {}

void SetAssociative::createCheckpoint(std::ostream &out) const noexcept {
  BACKUP_SCALAR(out, sectorsInCacheLine);
  BACKUP_SCALAR(out, setSize);
  BACKUP_SCALAR(out, waySize);

  uint64_t size = cacheline.size();
  BACKUP_SCALAR(out, size);

  for (auto &iter : cacheline) {
    iter.createCheckpoint(out);
  }

  size = lookupList.size();
  BACKUP_SCALAR(out, size);

  for (auto &iter : lookupList) {
    BACKUP_SCALAR(out, iter.first);
    BACKUP_SCALAR(out, iter.second);
  }

  size = flushList.size();
  BACKUP_SCALAR(out, size);

  for (auto &iter : flushList) {
    BACKUP_SCALAR(out, iter.tag);

    size = iter.lpnList.size();
    BACKUP_SCALAR(out, size);

    for (auto &iiter : iter.lpnList) {
      BACKUP_SCALAR(out, iiter.first);
      BACKUP_SCALAR(out, iiter.second);
    }
  }

  size = evictList.size();
  BACKUP_SCALAR(out, size);

  for (auto &iter : evictList) {
    BACKUP_SCALAR(out, iter.first);
    BACKUP_SCALAR(out, iter.second);
  }

  size = allocateList.size();
  BACKUP_SCALAR(out, size);

  for (auto &iter : allocateList) {
    BACKUP_SCALAR(out, iter.first);
    BACKUP_SCALAR(out, iter.second);
  }

  BACKUP_EVENT(out, eventLookupMemory);
  BACKUP_EVENT(out, eventLookupDone);
  BACKUP_EVENT(out, eventReadTag);
  BACKUP_EVENT(out, eventCacheDone);
}

void SetAssociative::restoreCheckpoint(std::istream &in) noexcept {
  uint32_t tmp32;

  RESTORE_SCALAR(in, tmp32);
  panic_if(tmp32 != sectorsInCacheLine, "Cacheline size mismatch.");

  RESTORE_SCALAR(in, tmp32);
  panic_if(tmp32 != setSize, "Set size mismatch.");

  RESTORE_SCALAR(in, tmp32);
  panic_if(tmp32 != waySize, "Way size mismatch.");

  uint64_t size;
  CacheLine line(sectorsInCacheLine);

  RESTORE_SCALAR(in, size);

  for (uint64_t i = 0; i < size; i++) {
    line.restoreCheckpoint(in);
  }

  RESTORE_SCALAR(in, size);

  for (uint64_t i = 0; i < size; i++) {
    LPN lpn;
    uint64_t tag;

    RESTORE_SCALAR(in, lpn);
    RESTORE_SCALAR(in, tag);

    lookupList.emplace(lpn, tag);
  }

  RESTORE_SCALAR(in, size);

  for (uint64_t i = 0; i < size; i++) {
    FlushRequest req;

    uint64_t ssize;
    LineInfo info;
    LPN lpn;

    RESTORE_SCALAR(in, req.tag);
    RESTORE_SCALAR(in, ssize);

    for (uint64_t j = 0; j < ssize; j++) {
      RESTORE_SCALAR(in, lpn);
      RESTORE_SCALAR(in, info);

      req.lpnList.emplace(lpn, info);
    }

    flushList.emplace_back(req);
  }

  RESTORE_SCALAR(in, size);

  for (uint64_t i = 0; i < size; i++) {
    LPN lpn;
    LineInfo info;

    RESTORE_SCALAR(in, lpn);
    RESTORE_SCALAR(in, info);

    evictList.emplace(lpn, info);
  }

  RESTORE_SCALAR(in, size);

  for (uint64_t i = 0; i < size; i++) {
    uint64_t tag;

    RESTORE_SCALAR(in, tmp32);
    RESTORE_SCALAR(in, tag);

    allocateList.emplace(tmp32, tag);
  }

  RESTORE_EVENT(in, eventLookupMemory);
  RESTORE_EVENT(in, eventLookupDone);
  RESTORE_EVENT(in, eventReadTag);
  RESTORE_EVENT(in, eventCacheDone);
}

}  // namespace SimpleSSD::ICL
