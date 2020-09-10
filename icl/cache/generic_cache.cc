// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "icl/cache/generic_cache.hh"

#include "icl/cache/ring_buffer.hh"
#include "icl/cache/set_associative.hh"
#include "icl/manager/abstract_manager.hh"
#include "util/algorithm.hh"

namespace SimpleSSD::ICL::Cache {

GenericCache::GenericCache(ObjectData &o, Manager::AbstractManager *m,
                           FTL::Parameter *p)
    : AbstractCache(o, m, p), dirtyLines(0), pendingEviction(0) {
  auto mode = (Config::Mode)readConfigUint(Section::InternalCache,
                                           Config::Key::CacheMode);

  switch (mode) {
    case Config::Mode::SetAssociative:
      tagArray = new SetAssociative(o, m, p);

      break;
    case Config::Mode::RingBuffer:
      tagArray = new RingBuffer(o, m, p);

      break;
    default:
      panic("Unexpected tag array for generic cache.");

      break;
  }

  totalTags = tagArray->getArraySize();
  logid = tagArray->getLogID();

  // Dirty pages threshold
  evictThreshold =
      readConfigFloat(Section::InternalCache, Config::Key::EvictThreshold) *
      totalTags;

  // Create events
  eventLookupDone =
      createEvent([this](uint64_t, uint64_t d) { manager->lookupDone(d); },
                  "ICL::GenericCache::eventLookupDone");
  eventCacheDone =
      createEvent([this](uint64_t, uint64_t d) { manager->cacheDone(d); },
                  "ICL::GenericCache::eventCacheDone");

  tagArray->init(pagesToEvict, eventLookupDone, eventCacheDone);
}

GenericCache::~GenericCache() {
  delete tagArray;
}

void GenericCache::tryLookup(LPN lpn, bool flush) {
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

void GenericCache::tryAllocate(LPN lpn) {
  for (auto iter = allocateList.begin(); iter != allocateList.end(); ++iter) {
    auto req = getSubRequest(*iter);

    if (tagArray->checkAllocatable(lpn, req)) {
      allocateList.erase(iter);

      allocate(req);

      break;
    }
  }
}

void GenericCache::lookup(HIL::SubRequest *sreq) {
  CPU::Function fstat;
  CPU::markFunction(fstat);

  LPN lpn = sreq->getLPN();
  CacheTag *ctag;

  fstat += tagArray->getValidLine(lpn, &ctag);

  if (ctag == nullptr) {
    // Check pending miss
    auto iter = missList.find(lpn);

    // Miss, we need allocation
    if (iter == missList.end()) {
      debugprint(logid,
                 "LOOKUP | REQ %7" PRIu64 ":%-3u | LPN %" PRIu64 " | Not found",
                 sreq->getParentTag(), sreq->getTagForLog(), lpn);

      sreq->setAllocate();
      sreq->setMiss();

      missList.emplace(lpn);
    }
    else {
      // Oh, we need to wait
      debugprint(logid,
                 "LOOKUP | REQ %7" PRIu64 ":%-3u | LPN %" PRIu64
                 " | Miss conflict",
                 sreq->getParentTag(), sreq->getTagForLog(), lpn);

      // Don't add to pending list when read-ahead/prefetch
      if (UNLIKELY(sreq->isICLRequest())) {
        // Call completion
        manager->cacheDone(sreq->getTag());
      }
      else {
        missConflictList.emplace(lpn, sreq->getTag());
      }

      return;
    }
  }
  else {
    debugprint(logid, "LOOKUP | REQ %7" PRIu64 ":%-3u | LPN %" PRIu64 " | %s",
               sreq->getParentTag(), sreq->getTagForLog(), lpn,
               tagArray->print(ctag).c_str());

    sreq->setDRAMAddress(tagArray->getDataAddress(ctag));

    // Check NAND/DMA is pending
    if (tagArray->checkPending(ctag)) {
      debugprint(logid,
                 "LOOKUP | REQ %7" PRIu64 ":%-3u | LPN %" PRIu64 " | Pending",
                 sreq->getParentTag(), sreq->getTagForLog(), lpn);

      // We need to stall this lookup
      lookupList.emplace(ctag->tag, sreq->getTag());

      // TODO: Memory/FW latency is missing
      return;
    }

    auto opcode = sreq->getOpcode();

    // Check valid bits
    Bitset test(sectorsInPage);

    updateSkip(test, sreq);

    // Update
    ctag->accessedAt = getTick();

    if (opcode == HIL::Operation::Write ||
        opcode == HIL::Operation::WriteZeroes) {
      ctag->validbits |= test;
    }
    else {
      test &= ctag->validbits;

      if (test.none()) {
        sreq->setMiss();
      }
    }
  }

  scheduleFunction(CPU::CPUGroup::InternalCache,
                   tagArray->getLookupMemoryEvent(), sreq->getTag(), fstat);
}

void GenericCache::flush(HIL::SubRequest *sreq) {
  CPU::Function fstat;
  CPU::markFunction(fstat);

  WritebackRequest wbreq;
  std::vector<Manager::FlushContext> list;

  LPN slpn = sreq->getOffset();
  uint32_t nlp = sreq->getLength();

  debugprint(logid, "FLUSH  | REQ %7" PRIu64 ":%-3u | LPN %" PRIu64 " + %u",
             sreq->getParentTag(), sreq->getTagForLog(), slpn, nlp);

  tagArray->collectFlushable(slpn, nlp, wbreq);

  auto ret = writebackList.emplace_back(std::move(wbreq));

  makeFlushContext(ret, list);

  ret.tag = sreq->getTag();
  ret.drainTag = manager->drain(list);
  ret.flush = true;

  scheduleFunction(CPU::CPUGroup::InternalCache,
                   tagArray->getReadAllMemoryEvent(), ret.tag, fstat);
}

void GenericCache::erase(HIL::SubRequest *sreq) {
  CPU::Function fstat;
  CPU::markFunction(fstat);

  LPN slpn = sreq->getOffset();
  uint32_t nlp = sreq->getLength();

  debugprint(logid, "ERASE  | REQ %7" PRIu64 ":%-3u | LPN %" PRIu64 " + %u",
             sreq->getParentTag(), sreq->getTagForLog(), slpn, nlp);

  fstat += tagArray->erase(slpn, nlp);

  scheduleFunction(CPU::CPUGroup::InternalCache,
                   tagArray->getReadAllMemoryEvent(), sreq->getTag(), fstat);
}

void GenericCache::allocate(HIL::SubRequest *sreq) {
  CPU::Function fstat;
  CPU::markFunction(fstat);

  LPN lpn = sreq->getLPN();
  bool evict = false;
  CacheTag *ctag;

  fstat += tagArray->getAllocatableLine(lpn, &ctag);

  Event eid = eventCacheDone;

  // Try allocate
  if (ctag == nullptr) {
    debugprint(logid,
               "ALLOC  | REQ %7" PRIu64 ":%-3u | LPN %" PRIu64 " | Pending",
               sreq->getParentTag(), sreq->getTagForLog(), lpn);

    // Insert into pending queue
    allocateList.emplace_back(sreq->getTag());

    evict = true;
    eid = InvalidEventID;
  }
  else {
    debugprint(logid, "ALLOC  | REQ %7" PRIu64 ":%-3u | LPN %" PRIu64 " | %s",
               sreq->getParentTag(), sreq->getTagForLog(), lpn,
               tagArray->print(ctag).c_str());

    sreq->setDRAMAddress(tagArray->getDataAddress(ctag));

    // Fill cacheline
    ctag->data = 0;  // Clear other bits
    ctag->valid = true;
    ctag->tag = lpn;
    ctag->insertedAt = getTick();
    ctag->accessedAt = getTick();

    // Partial update only if write
    auto opcode = sreq->getOpcode();

    if (opcode == HIL::Operation::Write ||
        opcode == HIL::Operation::WriteZeroes) {
      dirtyLines++;

      ctag->dirty = true;

      updateSkip(ctag->validbits, sreq);
    }
    else if (opcode == HIL::Operation::Read) {
      ctag->nvmPending = true;  // Read is triggered immediately
      ctag->validbits.set();
    }

    eid = tagArray->getWriteOneMemoryEvent();

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

    if (dirtyLines >= evictThreshold + pendingEviction) {
      evict = true;
      lpn = InvalidLPN;
    }
  }

  if (evict && (pendingEviction < pagesToEvict || eid == InvalidEventID)) {
    // Perform eviction
    WritebackRequest wbreq;
    std::vector<Manager::FlushContext> list;

    tagArray->collectEvictable(lpn, wbreq);

    if (wbreq.lpnList.size() > 0) {
      eid = tagArray->getReadAllMemoryEvent();

      auto ret = writebackList.emplace_back(std::move(wbreq));

      makeFlushContext(ret, list);

      pendingEviction += list.size();

      ret.drainTag = manager->drain(list);
    }
  }

  // No memory access because we already do that in lookup phase
  scheduleFunction(CPU::CPUGroup::InternalCache, eid, sreq->getTag(), fstat);
}

void GenericCache::dmaDone(LPN lpn) {
  CacheTag *ctag;

  tagArray->getValidLine(lpn, &ctag);

  if (ctag) {
    ctag->dmaPending = false;

    // Lookup
    tryLookup(lpn);

    // Allocate
    tryAllocate(lpn);
  }
}

void GenericCache::nvmDone(LPN lpn, uint64_t tag, bool drain) {
  bool found = false;

  if (drain) {
    // Write
    for (auto iter = writebackList.begin(); iter != writebackList.end();
         ++iter) {
      uint64_t drainTagBegin = iter->drainTag - iter->lpnList.size();
      uint64_t drainTagEnd = iter->drainTag;

      if (tag > drainTagBegin && tag <= drainTagEnd) {
        auto fr = iter->lpnList.find(lpn);

        panic_if(fr == iter->lpnList.end(), "Cache write-back corrupted.");

        dirtyLines--;

        if (!iter->flush) {
          pendingEviction--;
        }

        fr->second->dirty = false;
        fr->second->nvmPending = false;

        iter->lpnList.erase(fr);

        if (iter->lpnList.size() == 0) {
          if (iter->flush) {
            manager->cacheDone(iter->tag);
          }

          writebackList.erase(iter);
        }

        break;
      }
    }
  }
  else {
    // Read
    CacheTag *ctag;

    tagArray->getValidLine(lpn, &ctag);

    panic_if(ctag == nullptr, "Cache corrupted.");

    ctag->nvmPending = false;
    ctag->validbits.set();
  }

  // Lookup
  tryLookup(lpn, found);

  // Allocate
  tryAllocate(lpn);
}

void GenericCache::getStatList(std::vector<Stat> &list,
                               std::string prefix) noexcept {
  list.emplace_back(prefix + "dirty.count", "Total dirty cachelines");
  list.emplace_back(prefix + "dirty.ratio", "Total dirty cacheline ratio");

  tagArray->getStatList(list, prefix);
}

void GenericCache::getStatValues(std::vector<double> &values) noexcept {
  values.emplace_back((double)dirtyLines);
  values.emplace_back((double)dirtyLines / totalTags);

  tagArray->getStatValues(values);
}

void GenericCache::resetStatValues() noexcept {
  // MUST NOT RESET dirtyLines
  tagArray->resetStatValues();
}

void GenericCache::createCheckpoint(std::ostream &out) const noexcept {
  AbstractCache::createCheckpoint(out);

  auto mode = (Config::Mode)readConfigUint(Section::InternalCache,
                                           Config::Key::CacheMode);

  BACKUP_SCALAR(out, mode);

  tagArray->createCheckpoint(out);

  BACKUP_SCALAR(out, dirtyLines);

  uint64_t size = lookupList.size();
  BACKUP_SCALAR(out, size);

  for (auto &iter : lookupList) {
    BACKUP_SCALAR(out, iter.first);
    BACKUP_SCALAR(out, iter.second);
  }

  size = writebackList.size();
  BACKUP_SCALAR(out, size);

  for (auto &iter : writebackList) {
    BACKUP_SCALAR(out, iter.tag);
    BACKUP_SCALAR(out, iter.drainTag);
    BACKUP_SCALAR(out, iter.flush);

    size = iter.lpnList.size();
    BACKUP_SCALAR(out, size);

    for (auto &iiter : iter.lpnList) {
      uint64_t offset = tagArray->getOffset(iiter.second);

      BACKUP_SCALAR(out, iiter.first);
      BACKUP_SCALAR(out, offset);
    }
  }

  size = allocateList.size();
  BACKUP_SCALAR(out, size);

  for (auto &iter : allocateList) {
    BACKUP_SCALAR(out, iter);
  }

  BACKUP_EVENT(out, eventLookupDone);
  BACKUP_EVENT(out, eventCacheDone);
}

void GenericCache::restoreCheckpoint(std::istream &in) noexcept {
  uint32_t tmp32;
  Config::Mode mode;

  AbstractCache::restoreCheckpoint(in);

  RESTORE_SCALAR(in, mode);

  panic_if(mode != (Config::Mode)readConfigUint(Section::InternalCache,
                                                Config::Key::CacheMode),
           "Cache type mismatch.");

  tagArray->restoreCheckpoint(in);
  totalTags = tagArray->getArraySize();

  RESTORE_SCALAR(in, dirtyLines);

  uint64_t size;

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
    WritebackRequest req;
    LPN lpn;
    uint64_t ssize;
    uint64_t offset;

    RESTORE_SCALAR(in, req.tag);
    RESTORE_SCALAR(in, req.drainTag);
    RESTORE_SCALAR(in, req.flush);

    RESTORE_SCALAR(in, ssize);

    for (uint64_t j = 0; j < ssize; j++) {
      RESTORE_SCALAR(in, lpn);
      RESTORE_SCALAR(in, offset);

      req.lpnList.emplace(lpn, tagArray->getTag(offset));
    }

    writebackList.emplace_back(req);
  }

  RESTORE_SCALAR(in, size);

  for (uint64_t i = 0; i < size; i++) {
    LPN lpn;

    RESTORE_SCALAR(in, lpn);

    allocateList.emplace_back(lpn);
  }

  RESTORE_EVENT(in, eventLookupDone);
  RESTORE_EVENT(in, eventCacheDone);
}

}  // namespace SimpleSSD::ICL::Cache