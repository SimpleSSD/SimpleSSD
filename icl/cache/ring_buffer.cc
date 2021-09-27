// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "icl/cache/ring_buffer.hh"

#include "icl/manager/abstract_manager.hh"

namespace SimpleSSD::ICL::Cache {

RingBuffer::RingBuffer(ObjectData &o, Manager::AbstractManager *m,
                       const FTL::Parameter *p)
    : AbstractTagArray(o, m, p) {
  auto policy = (Config::EvictPolicyType)readConfigUint(
      Section::InternalCache, Config::Key::EvictPolicy);

  cacheTagSize = 8 + DIVCEIL(sectorsInPage, 8);
  cacheDataSize = p->pageSize;

  // Allocate cachelines
  uint64_t cacheSize =
      readConfigUint(Section::InternalCache, Config::Key::CacheSize);

  totalEntries = MAX(cacheSize / cacheDataSize, p->parallelismLevel[0]);

  cacheline.reserve(totalEntries);

  for (uint64_t i = 0; i < totalEntries; i++) {
    cacheline.emplace_back(CacheTag(sectorsInPage));
  }

  cacheSize = totalEntries * cacheDataSize;  // Recalculate

  debugprint(Log::DebugID::ICL_RingBuffer,
             "CREATE | Line size %u | Capacity %" PRIu64, cacheDataSize,
             cacheSize);

  // Allocate memory
  cacheTagSize = 8 + DIVCEIL(sectorsInPage, 8);

  uint64_t totalTagSize = cacheTagSize * totalEntries;

  /// Tag first
  if (object.memory->allocate(totalTagSize, Memory::MemoryType::SRAM, "",
                              true) == 0) {
    cacheTagBaseAddress = object.memory->allocate(
        totalTagSize, Memory::MemoryType::SRAM, "ICL::RingBuffer::Tag");
  }
  else {
    cacheTagBaseAddress = object.memory->allocate(
        totalTagSize, Memory::MemoryType::DRAM, "ICL::RingBuffer::Tag");
  }

  /// Data next
  if (object.memory->allocate(cacheSize, Memory::MemoryType::SRAM, "", true) ==
      0) {
    cacheDataBaseAddress = object.memory->allocate(
        cacheSize, Memory::MemoryType::SRAM, "ICL::RingBuffer::Data");
  }
  else {
    cacheDataBaseAddress = object.memory->allocate(
        cacheSize, Memory::MemoryType::DRAM, "ICL::RingBuffer::Data");
  }

  // Create victim selection
  switch (policy) {
    case Config::EvictPolicyType::FIFO:
      evictFunction = [this](uint64_t &i) -> CPU::Function {
        return fifoEviction(i);
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
      evictFunction = [this](uint64_t &i) -> CPU::Function {
        return lruEviction(i);
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
  }

  // Create events
  eventReadTagAll =
      createEvent([this](uint64_t, uint64_t d) { readAll(d, eventCacheDone); },
                  "ICL::RingBuffer::eventReadTag");
}

RingBuffer::~RingBuffer() {}

CPU::Function RingBuffer::fifoEviction(uint64_t &idx) {
  CPU::Function fstat;
  CPU::markFunction(fstat);

  idx = totalEntries;

  uint64_t min = std::numeric_limits<uint64_t>::max();

  for (auto &iter : tagHashTable) {
    auto &line = cacheline.at(iter.second);

    if (line.valid && line.insertedAt < min && !line.dmaPending &&
        !line.nvmPending) {
      min = line.insertedAt;
      idx = iter.second;
    }
  }

  return fstat;
}

CPU::Function RingBuffer::lruEviction(uint64_t &idx) {
  CPU::Function fstat;
  CPU::markFunction(fstat);

  idx = totalEntries;

  uint64_t min = std::numeric_limits<uint64_t>::max();

  for (auto &iter : tagHashTable) {
    auto &line = cacheline.at(iter.second);

    if (line.valid && line.accessedAt < min && !line.dmaPending &&
        !line.nvmPending) {
      min = line.accessedAt;
      idx = iter.second;
    }
  }

  return fstat;
}

CPU::Function RingBuffer::getValidLine(LPN lpn, uint64_t &idx) {
  CPU::Function fstat;
  CPU::markFunction(fstat);

  auto iter = tagHashTable.find(lpn);

  if (iter == tagHashTable.end()) {
    idx = totalEntries;
  }
  else {
    idx = iter->second;
  }

  return fstat;
}

CPU::Function RingBuffer::getEmptyLine(uint64_t &idx) {
  CPU::Function fstat;
  CPU::markFunction(fstat);

  if (tagHashTable.size() == totalEntries) {
    idx = totalEntries;
  }
  else {
    for (auto iter = cacheline.begin(); iter != cacheline.end(); ++iter) {
      if (!iter->valid) {
        idx = std::distance(cacheline.begin(), iter);

        break;
      }
    }
  }

  return fstat;
}

void RingBuffer::getCleanLine(uint64_t &idx) {
  idx = totalEntries;

  for (auto &iter : tagHashTable) {
    auto &line = cacheline.at(iter.second);

    if (!line.dirty && !line.dmaPending && !line.nvmPending) {
      if (idx == totalEntries) {
        idx = iter.second;
      }
      else {
        idx = compareFunction(iter.second, idx);
      }
    }
  }
}

void RingBuffer::readAll(uint64_t tag, Event eid) {
  object.memory->read(cacheTagBaseAddress, cacheTagSize * totalEntries, eid,
                      tag);
}

uint64_t RingBuffer::getArraySize() {
  return cacheline.size();
}

uint64_t RingBuffer::getDataAddress(CacheTag *ctag) {
  auto offset = getOffset(ctag);

  return makeDataAddress(offset);
}

Event RingBuffer::getReadAllMemoryEvent() {
  return eventReadTagAll;
}

CPU::Function RingBuffer::erase(LPN slpn, uint32_t nlp) {
  CPU::Function fstat;
  CPU::markFunction(fstat);

  for (auto &iter : cacheline) {
    if (iter.valid && slpn <= iter.tag && iter.tag < slpn + nlp) {
      iter.data = 0;
      iter.validbits.reset();

      auto hiter = tagHashTable.find(iter.tag);

      panic_if(hiter == tagHashTable.end(), "Cache corrupted.");

      tagHashTable.erase(hiter);
    }
  }

  return fstat;
}

bool RingBuffer::checkAllocatable(LPN, HIL::SubRequest *) {
  return true;
}

void RingBuffer::collectEvictable(LPN, WritebackRequest &wbreq) {
  std::vector<uint64_t> collected(pagesToEvict, totalEntries);

  for (auto &iter : tagHashTable) {
    auto &line = cacheline.at(iter.second);

    if (line.valid && line.dirty && !line.dmaPending && !line.nvmPending) {
      uint32_t offset = line.tag % pagesToEvict;

      auto &idx = collected.at(offset);

      if (idx < totalEntries) {
        idx = compareFunction(idx, iter.second);
      }
      else {
        idx = iter.second;
      }
    }
  }

  // Prepare list
  wbreq.lpnList.reserve(pagesToEvict);

  for (auto &i : collected) {
    if (i < totalEntries) {
      auto &line = cacheline.at(i);

      line.nvmPending = true;

      wbreq.lpnList.emplace(line.tag, &line);
    }
  }
}

void RingBuffer::collectFlushable(LPN slpn, uint32_t nlp,
                                  WritebackRequest &wbreq) {
  for (auto &iter : cacheline) {
    if (iter.valid && !iter.nvmPending && !iter.dmaPending &&
        slpn <= iter.tag && iter.tag < slpn + nlp) {
      iter.nvmPending = true;

      wbreq.lpnList.emplace(iter.tag, &iter);
    }
  }
}

CPU::Function RingBuffer::getValidLine(LPN lpn, CacheTag **ctag) {
  uint64_t idx;

  auto fstat = getValidLine(lpn, idx);

  if (idx < totalEntries) {
    *ctag = getTag(idx);
  }
  else {
    *ctag = nullptr;
  }

  return fstat;
}

CPU::Function RingBuffer::getAllocatableLine(LPN lpn, CacheTag **ctag) {
  uint64_t idx;

  auto fstat = getEmptyLine(idx);

  if (idx == totalEntries) {
    getCleanLine(idx);
  }

  if (idx < totalEntries) {
    *ctag = getTag(idx);

    // Fill tag hashtable
    if ((*ctag)->valid) {
      // Remove previous
      auto iter = tagHashTable.find((*ctag)->tag);

      panic_if(iter == tagHashTable.end(), "Cache corrupted.");

      tagHashTable.erase(iter);
    }

    tagHashTable.emplace(lpn, idx);
  }
  else {
    *ctag = nullptr;
  }

  return fstat;
}

std::string RingBuffer::print(CacheTag *ctag) noexcept {
  uint64_t idx = getOffset(ctag);

  return std::to_string(idx);
}

uint64_t RingBuffer::getOffset(CacheTag *ctag) const noexcept {
  return ctag - cacheline.data();
}

CacheTag *RingBuffer::getTag(uint64_t offset) noexcept {
  return cacheline.data() + offset;
}

void RingBuffer::getStatList(std::vector<Stat> &, std::string) noexcept {}

void RingBuffer::getStatValues(std::vector<double> &) noexcept {}

void RingBuffer::resetStatValues() noexcept {}

void RingBuffer::createCheckpoint(std::ostream &out) const noexcept {
  BACKUP_SCALAR(out, totalEntries);

  for (auto &iter : cacheline) {
    iter.createCheckpoint(out);
  }

  uint64_t size = tagHashTable.size();
  BACKUP_SCALAR(out, size);

  for (auto &iter : tagHashTable) {
    BACKUP_SCALAR(out, iter.first);
    BACKUP_SCALAR(out, iter.second);
  }

  BACKUP_EVENT(out, eventReadTagAll);
}

void RingBuffer::restoreCheckpoint(std::istream &in) noexcept {
  uint64_t tmp64;

  RESTORE_SCALAR(in, tmp64);
  panic_if(tmp64 != totalEntries, "Cache size mismatch.");

  for (auto &iter : cacheline) {
    iter.restoreCheckpoint(in);
  }

  RESTORE_SCALAR(in, tmp64);

  for (uint64_t i = 0; i < tmp64; i++) {
    LPN lpn;
    uint64_t idx;

    RESTORE_SCALAR(in, lpn);
    RESTORE_SCALAR(in, idx);

    tagHashTable.emplace(lpn, idx);
  }

  RESTORE_EVENT(in, eventReadTagAll);
}

}  // namespace SimpleSSD::ICL::Cache
