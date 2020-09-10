// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "icl/cache/set_associative.hh"

#include "icl/manager/abstract_manager.hh"
#include "util/algorithm.hh"

namespace SimpleSSD::ICL::Cache {

SetAssociative::SetAssociative(ObjectData &o, Manager::AbstractManager *m,
                               const FTL::Parameter *p)
    : AbstractTagArray(o, m, p) {
  auto policy = (Config::EvictPolicyType)readConfigUint(
      Section::InternalCache, Config::Key::EvictPolicy);

  cacheTagSize = 8 + DIVCEIL(sectorsInPage, 8);
  cacheDataSize = p->pageSize;

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
    cacheline.emplace_back(CacheTag(sectorsInPage));
  }

  cacheSize = (uint64_t)setSize * waySize * cacheDataSize;

  debugprint(
      Log::DebugID::ICL_SetAssociative,
      "CREATE | Set size %u | Way size %u | Line size %u | Capacity %" PRIu64,
      setSize, waySize, cacheDataSize, cacheSize);

  // Allocate memory
  cacheDataBaseAddress = object.memory->allocate(
      cacheSize, Memory::MemoryType::DRAM, "ICL::SetAssociative::Data");

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

  // Create victim cacheline selection
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
  eventReadSet =
      createEvent([this](uint64_t, uint64_t d) { readSet(d, eventLookupDone); },
                  "ICL::SetAssociative::eventLookupMemory");
  eventReadAll =
      createEvent([this](uint64_t, uint64_t d) { readAll(d, eventCacheDone); },
                  "ICL::SetAssociative::eventReadTag");
  eventWriteOne = createEvent(
      [this](uint64_t, uint64_t d) {
        uint32_t set, way;
        auto sreq = getSubRequest(d);

        CacheTag *ctag;

        getValidLine(sreq->getLPN(), &ctag);

        panic_if(ctag == nullptr, "Unexpected LPN.");

        tagToLine(ctag, set, way);

        writeLine(d, set, way, eventCacheDone);
      },
      "ICL::SetAssociative::eventWriteOne");
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

    if (line.valid && !line.dirty && !line.dmaPending && !line.nvmPending) {
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

void SetAssociative::writeLine(uint64_t tag, uint32_t set, uint32_t way,
                               Event eid) {
  object.memory->write(makeTagAddress(set, way), cacheTagSize, eid, tag);
}

uint64_t SetAssociative::getArraySize() {
  return cacheline.size();
}

uint64_t SetAssociative::getDataAddress(CacheTag *ctag) {
  uint32_t set, way;

  tagToLine(ctag, set, way);

  return makeDataAddress(set, way);
}

Event SetAssociative::getLookupMemoryEvent() {
  return eventReadSet;
}

Event SetAssociative::getReadAllMemoryEvent() {
  return eventReadAll;
}

Event SetAssociative::getWriteOneMemoryEvent() {
  return eventWriteOne;
}

CPU::Function SetAssociative::erase(LPN slpn, uint32_t nlp) {
  CPU::Function fstat;
  CPU::markFunction(fstat);

  for (auto &iter : cacheline) {
    if (iter.valid && slpn <= iter.tag && iter.tag < slpn + nlp) {
      iter.data = 0;
      iter.validbits.reset();
    }
  }

  return fstat;
}

bool SetAssociative::checkAllocatable(LPN lpn, HIL::SubRequest *sreq) {
  return getSetIdx(lpn) == getSetIdx(sreq->getLPN());
}

void SetAssociative::collectEvictable(LPN lpn, WritebackRequest &wbreq) {
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
  if (lpn) {
    uint32_t curSet = getSetIdx(lpn);
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

      // Maybe some cachelines are in eviction
      for (way = 0; way < waySize; way++) {
        auto &line = cacheline.at(curSet * waySize + way);

        if (line.nvmPending) {
          found = true;

          break;
        }
      }

      if (!found) {
        evictFunction(curSet, way);

        if (way != waySize) {
          uint64_t i = curSet * waySize + way;
          auto &line = cacheline.at(i);
          uint32_t offset = line.tag % pagesToEvict;

          collected.at(offset) = i;
        }
      }
    }
  }

  // Prepare list
  wbreq.lpnList.reserve(collected.size());

  for (auto &i : collected) {
    if (i < size) {
      auto &line = cacheline.at(i);

      line.nvmPending = true;

      wbreq.lpnList.emplace(line.tag, &line);
    }
  }
}

void SetAssociative::collectFlushable(LPN slpn, uint32_t nlp,
                                      WritebackRequest &wbreq) {
  for (auto &iter : cacheline) {
    if (iter.valid && !iter.nvmPending && !iter.dmaPending &&
        slpn <= iter.tag && iter.tag < slpn + nlp) {
      iter.nvmPending = true;

      wbreq.lpnList.emplace(iter.tag, &iter);
    }
  }
}

CPU::Function SetAssociative::getValidLine(LPN lpn, CacheTag **ctag) {
  uint32_t set = getSetIdx(lpn);
  uint32_t way;

  auto fstat = getValidWay(set, way);

  if (way < waySize) {
    *ctag = lineToTag(set, way);
  }
  else {
    *ctag = nullptr;
  }

  return fstat;
}

CPU::Function SetAssociative::getAllocatableLine(LPN lpn, CacheTag **ctag) {
  uint32_t set = getSetIdx(lpn);
  uint32_t way;

  auto fstat = getEmptyWay(set, way);

  if (way == waySize) {
    getCleanWay(set, way);
  }

  if (way < waySize) {
    *ctag = lineToTag(set, way);
  }
  else {
    *ctag = nullptr;
  }

  return fstat;
}

std::string SetAssociative::print(CacheTag *ctag) noexcept {
  uint32_t set, way;
  std::string ret;

  tagToLine(ctag, set, way);

  ret += '(';
  ret += std::to_string(set);
  ret += ", ";
  ret += std::to_string(way);
  ret += ')';

  return ret;
}

uint64_t SetAssociative::getOffset(CacheTag *ctag) const noexcept {
  return ctag - cacheline.data();
}

CacheTag *SetAssociative::getTag(uint64_t offset) noexcept {
  return cacheline.data() + offset;
}

void SetAssociative::getStatList(std::vector<Stat> &, std::string) noexcept {}

void SetAssociative::getStatValues(std::vector<double> &) noexcept {}

void SetAssociative::resetStatValues() noexcept {}

void SetAssociative::createCheckpoint(std::ostream &out) const noexcept {
  BACKUP_SCALAR(out, setSize);
  BACKUP_SCALAR(out, waySize);

  for (auto &iter : cacheline) {
    iter.createCheckpoint(out);
  }

  BACKUP_EVENT(out, eventReadAll);
  BACKUP_EVENT(out, eventReadSet);
  BACKUP_EVENT(out, eventWriteOne);
}

void SetAssociative::restoreCheckpoint(std::istream &in) noexcept {
  uint32_t tmp32;

  RESTORE_SCALAR(in, tmp32);
  panic_if(tmp32 != setSize, "Set size mismatch.");

  RESTORE_SCALAR(in, tmp32);
  panic_if(tmp32 != waySize, "Way size mismatch.");

  for (auto &iter : cacheline) {
    iter.restoreCheckpoint(in);
  }

  RESTORE_EVENT(in, eventReadAll);
  RESTORE_EVENT(in, eventReadSet);
  RESTORE_EVENT(in, eventWriteOne);
}

}  // namespace SimpleSSD::ICL::Cache
