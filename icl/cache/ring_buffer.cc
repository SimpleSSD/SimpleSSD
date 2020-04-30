// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "icl/cache/ring_buffer.hh"

#include "icl/manager/abstract_manager.hh"

namespace SimpleSSD::ICL {

RingBuffer::RingBuffer(ObjectData &o, AbstractManager *m, FTL::Parameter *p)
    : AbstractCache(o, m, p), dirtyLines(0) {
  auto policy = (Config::EvictPolicyType)readConfigUint(
      Section::InternalCache, Config::Key::EvictPolicy);

  cacheDataSize = parameter->pageSize;

  // Allocate cachelines
  uint64_t cacheSize =
      readConfigUint(Section::InternalCache, Config::Key::CacheSize);

  uint64_t totalEntries =
      MIN(cacheSize / cacheDataSize, p->parallelismLevel[0]);

  cacheline.reserve(totalEntries);

  for (uint64_t i = 0; i < totalEntries; i++) {
    cacheline.emplace_back(CacheLine(sectorsInPage));
  }

  cacheSize = totalEntries * cacheDataSize;  // Recalculate

  debugprint(Log::DebugID::ICL_RingBuffer,
             "CREATE | Line size %u | Capacity %" PRIu64, cacheDataSize,
             cacheSize);

  // Dirty cacheline threshold
  evictThreshold =
      readConfigFloat(Section::InternalCache, Config::Key::EvictThreshold) *
      totalEntries;

  // Allocate memory
  cacheTagSize = 8 + DIVCEIL(sectorsInPage, 8);

  uint64_t totalTagSize = cacheTagSize * totalEntries;

  /// Tag first
  if (object.memory->allocate(cacheTagSize, Memory::MemoryType::SRAM, "",
                              true) == 0) {
    cacheTagBaseAddress = object.memory->allocate(
        cacheTagSize, Memory::MemoryType::SRAM, "ICL::RingBuffer::Tag");
  }
  else {
    cacheTagBaseAddress = object.memory->allocate(
        cacheTagSize, Memory::MemoryType::DRAM, "ICL::RingBuffer::Tag");
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
      break;
    case Config::EvictPolicyType::LRU:
      break;
  }

  // Create events
  eventLookupDone =
      createEvent([this](uint64_t, uint64_t d) { manager->lookupDone(d); },
                  "ICL::RingBuffer::eventLookupDone");
  eventCacheDone =
      createEvent([this](uint64_t, uint64_t d) { manager->cacheDone(d); },
                  "ICL::RingBuffer::eventCacheDone");
}

RingBuffer::~RingBuffer() {}

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

void RingBuffer::lookup(HIL::SubRequest *sreq) {
  CPU::Function fstat;
  CPU::markFunction(fstat);

  LPN lpn = sreq->getLPN();
  uint64_t idx;

  fstat += getValidLine(lpn, idx);

  if (idx == totalEntries) {
    // Check pending miss
    auto iter = missList.find(lpn);

    // Miss, we need allocation
    if (iter == missList.end()) {
      debugprint(Log::DebugID::ICL_RingBuffer,
                 "LOOKUP | REQ %7" PRIu64 ":%-3u | LPN %" PRIu64 " | Not found",
                 sreq->getParentTag(), sreq->getTagForLog(), lpn);

      sreq->setAllocate();
      sreq->setMiss();

      missList.emplace(lpn);
    }
    else {
      // Wait for allocation
      debugprint(Log::DebugID::ICL_RingBuffer,
                 "LOOKUP | REQ %7" PRIu64 ":%-3u | LPN %" PRIu64
                 " | Miss conflict",
                 sreq->getParentTag(), sreq->getTagForLog(), lpn);

      missConflictList.emplace(lpn, sreq->getTag());

      return;
    }
  }
  else {
    debugprint(Log::DebugID::ICL_RingBuffer,
               "LOOKUP | REQ %7" PRIu64 ":%-3u | LPN %" PRIu64
               "| Line %" PRIu64,
               sreq->getParentTag(), sreq->getTagForLog(), lpn, idx);

    sreq->setDRAMAddress(makeDataAddress(idx));

    // Check DMA/NVM pending
    auto &line = cacheline.at(idx);
    auto opcode = sreq->getOpcode();

    if (line.dmaPending || line.nvmPending) {
      debugprint(Log::DebugID::ICL_RingBuffer,
                 "LOOKUP | REQ %7" PRIu64 ":%-3u | LPN %" PRIu64 "| Pending",
                 sreq->getParentTag(), sreq->getTagForLog(), lpn);

      lookupList.emplace(line.tag, sreq->getTag());

      return;
    }

    // Check valid bits
    Bitset test(sectorsInPage);

    updateSkip(test, sreq);

    // Update
    line.accessedAt = getTick();

    if (opcode == HIL::Operation::Write ||
        opcode == HIL::Operation::WriteZeroes) {
      line.validbits |= test;
    }
    else {
      test &= line.valid;

      if (test.none()) {
        sreq->setMiss();
      }
    }
  }

  // TODO: Add tag memory access
  scheduleFunction(CPU::CPUGroup::InternalCache, eventLookupDone,
                   sreq->getTag(), fstat);
}

}  // namespace SimpleSSD::ICL
