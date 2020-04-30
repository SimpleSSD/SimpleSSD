// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "icl/cache/ring_buffer.hh"

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
}

RingBuffer::~RingBuffer() {}

}  // namespace SimpleSSD::ICL
