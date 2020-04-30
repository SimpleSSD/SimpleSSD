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

  totalEntries = MAX(cacheSize / cacheDataSize, p->parallelismLevel[0]);

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
  eventLookupDone =
      createEvent([this](uint64_t, uint64_t d) { manager->lookupDone(d); },
                  "ICL::RingBuffer::eventLookupDone");
  eventCacheDone =
      createEvent([this](uint64_t, uint64_t d) { manager->cacheDone(d); },
                  "ICL::RingBuffer::eventCacheDone");
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

    if (!line.dirty) {
      if (idx == totalEntries) {
        idx = iter.second;
      }
      else {
        idx = compareFunction(iter.second, idx);
      }

      break;
    }
  }
}

void RingBuffer::readAll(uint64_t tag, Event eid) {
  object.memory->read(cacheTagBaseAddress, cacheTagSize * totalEntries, eid,
                      tag);
}

void RingBuffer::tryLookup(LPN lpn, bool flush) {
  auto iter = lookupList.find(lpn);

  if (iter != lookupList.end()) {
    if (flush) {
      // This was flush -> cacheline looked up was invalidated
      auto req = getSubRequest(iter->second);

      req->setAllocate();
      req->setMiss();
    }

    manager->lookupDone(iter->second);
    lookupList.erase(iter);
  }
}

void RingBuffer::tryAllocate() {
  if (allocateList.size() > 0) {
    // Try allocate again
    auto req = getSubRequest(allocateList.front());

    // Must erase first
    allocateList.pop_front();

    allocate(req);
  }
}

void RingBuffer::collect(std::vector<FlushContext> &list) {
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
  list.reserve(pagesToEvict);

  for (auto &i : collected) {
    if (i < totalEntries) {
      auto &line = cacheline.at(i);

      line.nvmPending = true;

      evictList.emplace(line.tag, i);
      list.emplace_back(FlushContext(line.tag, makeDataAddress(i)));
    }
  }
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
      test &= line.validbits;

      if (test.none()) {
        sreq->setMiss();
      }
    }
  }

  // TODO: Add tag memory access
  scheduleFunction(CPU::CPUGroup::InternalCache, eventLookupDone,
                   sreq->getTag(), fstat);
}

void RingBuffer::flush(HIL::SubRequest *sreq) {
  CPU::Function fstat;
  CPU::markFunction(fstat);

  std::vector<FlushContext> list;
  std::unordered_map<LPN, uint64_t> lpnList;

  LPN slpn = sreq->getOffset();
  uint32_t nlp = sreq->getLength();
  uint64_t i = 0;

  debugprint(Log::DebugID::ICL_RingBuffer,
             "FLUSH  | REQ %7" PRIu64 ":%-3u | LPN %" PRIu64 " + %u",
             sreq->getParentTag(), sreq->getTagForLog(), slpn, nlp);

  for (auto &iter : cacheline) {
    if (iter.valid && !iter.nvmPending && !iter.dmaPending &&
        slpn <= iter.tag && iter.tag < slpn + nlp) {
      iter.nvmPending = true;

      auto ret = list.emplace_back(iter.tag, makeDataAddress(i));

      ret.offset = iter.validbits.ctz() * minIO;
      ret.length = cacheDataSize - iter.validbits.clz() * minIO - ret.offset;

      lpnList.emplace(iter.tag, i);
    }

    i++;
  }

  auto ret = flushList.emplace_back(sreq->getTag(), std::move(lpnList));

  manager->drain(list);

  scheduleFunction(CPU::CPUGroup::InternalCache, eventReadTagAll,
                   sreq->getTag(), fstat);
}

void RingBuffer::erase(HIL::SubRequest *sreq) {
  CPU::Function fstat;
  CPU::markFunction(fstat);

  LPN slpn = sreq->getOffset();
  uint32_t nlp = sreq->getLength();

  debugprint(Log::DebugID::ICL_RingBuffer,
             "ERASE  | REQ %7" PRIu64 ":%-3u | LPN %" PRIu64 " + %u",
             sreq->getParentTag(), sreq->getTagForLog(), slpn, nlp);

  for (auto &iter : cacheline) {
    if (iter.valid && slpn <= iter.tag && iter.tag < slpn + nlp) {
      iter.data = 0;
      iter.validbits.reset();

      auto hiter = tagHashTable.find(iter.tag);

      panic_if(hiter == tagHashTable.end(), "Cache corrupted.");

      tagHashTable.erase(hiter);
    }
  }

  scheduleFunction(CPU::CPUGroup::InternalCache, eventReadTagAll,
                   sreq->getTag(), fstat);
}

void RingBuffer::allocate(HIL::SubRequest *sreq) {
  CPU::Function fstat;
  CPU::markFunction(fstat);

  LPN lpn = sreq->getLPN();
  uint64_t idx;
  bool evict = false;
  Event eid = eventCacheDone;

  // Try allocate
  fstat += getEmptyLine(idx);

  if (idx == totalEntries) {
    // Double-check with clean line
    getCleanLine(idx);
  }

  if (idx == totalEntries) {
    debugprint(Log::DebugID::ICL_RingBuffer,
               "ALLOC  | REQ %7" PRIu64 ":%-3u | LPN %" PRIu64 " | Pending",
               sreq->getParentTag(), sreq->getTagForLog(), lpn);

    // Insert into pending queue
    allocateList.emplace_back(sreq->getTag());

    evict = true;
    eid = InvalidEventID;
  }
  else {
    debugprint(Log::DebugID::ICL_RingBuffer,
               "ALLOC  | REQ %7" PRIu64 ":%-3u | LPN %" PRIu64
               " | Line %" PRIu64,
               sreq->getParentTag(), sreq->getTagForLog(), lpn, idx);

    // Set DRAM address
    sreq->setDRAMAddress(makeDataAddress(idx));

    // Fill cacheline
    auto &line = cacheline.at(idx);

    // Fill tag hashtable
    if (line.valid) {
      // Remove previous
      auto iter = tagHashTable.find(line.tag);

      panic_if(iter == tagHashTable.end(), "Cache corrupted.");

      tagHashTable.erase(iter);
    }

    tagHashTable.emplace(lpn, idx);

    line.data = 0;  // Clear other bits
    line.valid = true;
    line.tag = lpn;
    line.insertedAt = getTick();
    line.accessedAt = getTick();

    // Partial update only if write
    auto opcode = sreq->getOpcode();

    if (opcode == HIL::Operation::Write ||
        opcode == HIL::Operation::WriteZeroes) {
      dirtyLines++;

      line.dirty = true;

      updateSkip(line.validbits, sreq);
    }
    else if (opcode == HIL::Operation::Read) {
      line.nvmPending = true;  // Read is triggered immediately
      line.validbits.set();
    }

    // TODO: Fix me
    object.memory->write(makeTagAddress(idx), cacheTagSize, InvalidEventID,
                         sreq->getTag());

    if (dirtyLines >= evictThreshold + evictList.size()) {
      evict = true;
    }

    // Remove lpn from miss list
    auto iter = missList.find(lpn);

    if (iter != missList.end()) {
      // Check miss conflict list
      auto range = missConflictList.equal_range(lpn);

      for (auto iter = range.first; iter != range.second;) {
        auto req = getSubRequest(iter->second);

        // Retry lookup (must be hit)
        lookup(req);

        iter = missConflictList.erase(iter);
      }

      missList.erase(iter);
    }
  }

  if (evict && (evictList.size() < pagesToEvict || eid == InvalidEventID)) {
    // Perform eviction
    std::vector<FlushContext> list;

    collect(list);

    if (!list.empty()) {
      manager->drain(list);
    }
  }

  // No memory access because we already do that in lookup phase
  scheduleFunction(CPU::CPUGroup::InternalCache, eid, sreq->getTag(), fstat);
}

void RingBuffer::dmaDone(LPN lpn) {
  uint64_t idx;

  getValidLine(lpn, idx);

  if (idx < totalEntries) {
    auto &line = cacheline.at(idx);

    line.dmaPending = false;

    // Lookup
    tryLookup(lpn);

    // Allocate
    tryAllocate();
  }
}

void RingBuffer::nvmDone(LPN lpn, bool drain) {
  bool found = false;

  if (drain) {
    // Flush
    for (auto iter = flushList.begin(); iter != flushList.end(); iter++) {
      auto fr = iter->second.find(lpn);

      if (fr != iter->second.end()) {
        auto &line = cacheline.at(fr->second);

        // Not dirty now
        dirtyLines--;

        line.dirty = false;
        line.nvmPending = false;

        // Erase
        iter->second.erase(fr);

        if (iter->second.size() == 0) {
          manager->cacheDone(iter->first);

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
        auto &line = cacheline.at(iter->second);

        dirtyLines--;

        line.dirty = false;
        line.nvmPending = false;

        evictList.erase(iter);

        found = true;
      }
    }
  }
  else {
    // Read
    auto iter = tagHashTable.find(lpn);

    panic_if(iter == tagHashTable.end(), "Cache corrupted.");

    auto &line = cacheline.at(iter->second);

    line.nvmPending = false;
    line.validbits.set();
  }

  // Lookup
  tryLookup(lpn, found);

  // Allocate
  tryAllocate();
}

void RingBuffer::getStatList(std::vector<Stat> &list,
                             std::string prefix) noexcept {
  list.emplace_back(prefix + "dirty.count", "Total dirty cachelines");
  list.emplace_back(prefix + "dirty.ratio", "Total dirty cacheline ratio");
}

void RingBuffer::getStatValues(std::vector<double> &values) noexcept {
  values.emplace_back((double)dirtyLines);
  values.emplace_back((double)dirtyLines / totalEntries);
}

void RingBuffer::resetStatValues() noexcept {
  // MUST NOT RESET dirtyLines
}

void RingBuffer::createCheckpoint(std::ostream &out) const noexcept {
  AbstractCache::createCheckpoint(out);

  BACKUP_SCALAR(out, totalEntries);
  BACKUP_SCALAR(out, dirtyLines);

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
    BACKUP_SCALAR(out, iter.first);

    size = iter.second.size();
    BACKUP_SCALAR(out, size);

    for (auto &iiter : iter.second) {
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
    BACKUP_SCALAR(out, iter);
  }

  BACKUP_EVENT(out, eventReadTagAll);
  BACKUP_EVENT(out, eventLookupDone);
  BACKUP_EVENT(out, eventCacheDone);
}

void RingBuffer::restoreCheckpoint(std::istream &in) noexcept {
  uint32_t tmp64;

  AbstractCache::restoreCheckpoint(in);

  RESTORE_SCALAR(in, tmp64);
  panic_if(tmp64 != totalEntries, "Cache size mismatch.");

  RESTORE_SCALAR(in, dirtyLines);

  uint64_t size;
  CacheLine line(sectorsInPage);

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
    uint64_t tag;
    std::unordered_map<LPN, uint64_t> list;

    uint64_t ssize;
    uint64_t idx;
    LPN lpn;

    RESTORE_SCALAR(in, tag);
    RESTORE_SCALAR(in, ssize);

    for (uint64_t j = 0; j < ssize; j++) {
      RESTORE_SCALAR(in, lpn);
      RESTORE_SCALAR(in, idx);

      list.emplace(lpn, idx);
    }

    flushList.emplace_back(tag, list);
  }

  RESTORE_SCALAR(in, size);

  for (uint64_t i = 0; i < size; i++) {
    LPN lpn;
    uint64_t idx;

    RESTORE_SCALAR(in, lpn);
    RESTORE_SCALAR(in, idx);

    evictList.emplace(lpn, idx);
  }

  RESTORE_SCALAR(in, size);

  for (uint64_t i = 0; i < size; i++) {
    uint64_t tag;

    RESTORE_SCALAR(in, tag);

    allocateList.emplace_back(tag);
  }

  RESTORE_EVENT(in, eventReadTagAll);
  RESTORE_EVENT(in, eventLookupDone);
  RESTORE_EVENT(in, eventCacheDone);
}

}  // namespace SimpleSSD::ICL
