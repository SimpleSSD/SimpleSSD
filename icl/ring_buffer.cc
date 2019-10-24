// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "icl/ring_buffer.hh"

#include <algorithm>
#include <cstddef>
#include <limits>

#include "util/algorithm.hh"

namespace SimpleSSD::ICL {

RingBuffer::PrefetchTrigger::PrefetchTrigger(const uint64_t c, const uint64_t r)
    : prefetchCount(c),
      prefetchRatio(r),
      requestCounter(0),
      requestCapacity(0),
      lastAddress(std::numeric_limits<uint64_t>::max()),
      trigger(false) {}

void RingBuffer::PrefetchTrigger::update(LPN addr, uint32_t size) {
  // New request arrived, check sequential
  if (addr == lastAddress) {
    requestCounter++;
    requestCapacity += size;
  }
  else {
    // Reset
    requestCounter = 0;
    requestCapacity = 0;
  }

  lastAddress = addr + size;

  if (requestCounter >= prefetchCount && requestCapacity >= prefetchRatio) {
    trigger = true;
  }
  else {
    trigger = false;
  }
}

bool RingBuffer::PrefetchTrigger::triggered() {
  return trigger;
}

RingBuffer::RingBuffer(ObjectData &o, CommandManager *m, FTL::FTL *p)
    : AbstractCache(o, m, p),
      pageSize(p->getInfo()->pageSize),
      iobits(pageSize / minIO),
      trigger(
          readConfigUint(Section::InternalCache, Config::Key::PrefetchCount),
          readConfigUint(Section::InternalCache, Config::Key::PrefetchRatio) *
              pageSize),
      readTriggered(false),
      writeTriggered(false),
      readWaitsEviction(0),
      flushTag({InvalidEventID, 0}) {
  auto param = pFTL->getInfo();

  triggerThreshold =
      readConfigFloat(Section::InternalCache, Config::Key::EvictThreshold);

  // Make granularity
  switch ((Config::Granularity)readConfigUint(Section::InternalCache,
                                              Config::Key::EvictMode)) {
    case Config::Granularity::SuperpageLevel:
      evictPages = param->superpage;

      break;
    case Config::Granularity::AllLevel:
      evictPages = param->parallelism;

      break;
    default:
      panic("Invalid eviction granularity.");

      break;
  }

  // Calculate FTL's write granularity
  minPages = param->superpage;

  if (minPages == 1) {
    noPageLimit = true;

    minPages = param->parallelismLevel[0];  // First parallelism level
  }
  else {
    noPageLimit = false;
  }

  switch ((Config::Granularity)readConfigUint(Section::InternalCache,
                                              Config::Key::PrefetchMode)) {
    case Config::Granularity::SuperpageLevel:
      prefetchPages = param->superpage;

      break;
    case Config::Granularity::AllLevel:
      prefetchPages = param->parallelism;

      break;
    default:
      panic("Invalid prefetch granularity.");

      break;
  }

  enabled = readConfigBoolean(Section::InternalCache, Config::Key::EnableCache);
  prefetchEnabled =
      readConfigBoolean(Section::InternalCache, Config::Key::EnablePrefetch);

  uint64_t totalCapacity =
      readConfigUint(Section::InternalCache, Config::Key::CacheSize);
  maxEntryCount = totalCapacity / minPages / pageSize;

  setCache(enabled);

  debugprint(Log::DebugID::ICL_RingBuffer, "CREATE  | Capacity %" PRIu64,
             totalCapacity);
  debugprint(Log::DebugID::ICL_RingBuffer,
             "CREATE  | Eviction granularity %u pages", evictPages);

  // Allocate DRAM for data
  dataAddress = object.dram->allocate(totalCapacity);

  // Create evict policy
  evictPolicy = (Config::EvictModeType)readConfigUint(Section::InternalCache,
                                                      Config::Key::EvictMode);

  switch (evictPolicy) {
    case Config::EvictModeType::Random:
      mtengine = std::mt19937(rd());

      chooseEntry = [this](SelectionMode sel) {
        std::vector<CacheEntry::iterator> list;

        if (sel == SelectionMode::All) {
          for (auto iter = cacheEntry.begin(); iter != cacheEntry.end();
               ++iter) {
            if (isDirty(iter->second.list)) {
              list.emplace_back(iter);
            }
          }

          std::uniform_int_distribution<uint32_t> dist(0, list.size() - 1);

          return list.at(dist(mtengine));
        }
        else if (sel == SelectionMode::FullSized) {
          for (auto iter = cacheEntry.begin(); iter != cacheEntry.end();
               ++iter) {
            if (isFullSizeDirty(iter->second.list)) {
              list.emplace_back(iter);
            }
          }

          std::uniform_int_distribution<uint32_t> dist(0, list.size() - 1);

          return list.at(dist(mtengine));
        }
        else {
          for (auto iter = cacheEntry.begin(); iter != cacheEntry.end();
               ++iter) {
            if (isClean(iter->second.list)) {
              list.emplace_back(iter);
            }
          }

          std::uniform_int_distribution<uint32_t> dist(0, list.size() - 1);

          return list.at(dist(mtengine));
        }
      };

      break;
    case Config::EvictModeType::FIFO:
      chooseEntry = [this](SelectionMode sel) {
        uint16_t diff = 0;
        auto idx = cacheEntry.end();

        // Find line with largest difference
        for (auto iter = cacheEntry.begin(); iter != cacheEntry.end(); ++iter) {
          if (diff < (uint16_t)(clock - iter->second.insertedAt)) {
            if (sel == SelectionMode::All) {
              if (!isDirty(iter->second.list)) {
                continue;
              }
            }
            else if (sel == SelectionMode::FullSized) {
              if (!isFullSizeDirty(iter->second.list)) {
                continue;
              }
            }
            else {
              if (!isClean(iter->second.list)) {
                continue;
              }
            }

            diff = clock - iter->second.insertedAt;
            idx = iter;
          }
        }

        return idx;
      };

      break;
    case Config::EvictModeType::LRU:
      chooseEntry = [this](SelectionMode sel) {
        uint16_t diff = 0;
        auto idx = cacheEntry.end();

        // Find line with largest difference
        for (auto iter = cacheEntry.begin(); iter != cacheEntry.end(); ++iter) {
          if (diff < (uint16_t)(clock - iter->second.accessedAt)) {
            if (sel == SelectionMode::All) {
              if (!isDirty(iter->second.list)) {
                continue;
              }
            }
            else if (sel == SelectionMode::FullSized) {
              if (!isFullSizeDirty(iter->second.list)) {
                continue;
              }
            }
            else {
              if (!isClean(iter->second.list)) {
                continue;
              }
            }

            diff = clock - iter->second.accessedAt;
            idx = iter;
          }
        }

        return idx;
      };

      break;
  }

  memset(&stat, 0, sizeof(stat));

  // Make events
  eventReadWorker = createEvent([this](uint64_t t, uint64_t) { readWorker(t); },
                                "ICL::RingBuffer::eventReadWorker");
  eventReadWorkerDoFTL =
      createEvent([this](uint64_t, uint64_t) { readWorker_doFTL(); },
                  "ICL::RingBuffer::eventReadWorkerDoFTL");
  eventReadWorkerDone =
      createEvent([this](uint64_t t, uint64_t d) { readWorker_done(t, d); },
                  "ICL::RingBuffer::eventReadWorkerDone");
  eventWriteWorker =
      createEvent([this](uint64_t t, uint64_t) { writeWorker(t); },
                  "ICL::RingBuffer::eventWriteWorker");
  eventWriteWorkerDoFTL =
      createEvent([this](uint64_t, uint64_t) { writeWorker_doFTL(); },
                  "ICL::RingBuffer::eventWriteWorkerDoFTL");
  eventWriteWorkerDone =
      createEvent([this](uint64_t t, uint64_t d) { writeWorker_done(t, d); },
                  "ICL::RingBuffer::eventWriteWorkerDone");
  eventReadPreCPUDone =
      createEvent([this](uint64_t, uint64_t d) { read_findDone(d); },
                  "ICL::RingBuffer::eventReadPreCPUDone");
  eventReadDRAMDone =
      createEvent([this](uint64_t, uint64_t d) { read_done(d); },
                  "ICL::RingBuffer::eventReadDRAMDone");
  eventWritePreCPUDone =
      createEvent([this](uint64_t, uint64_t d) { write_findDone(d); },
                  "ICL::RingBuffer::eventWritePreCPUDone");
  eventWriteDRAMDone =
      createEvent([this](uint64_t, uint64_t d) { write_done(d); },
                  "ICL::RingBuffer::eventWriteDRAMDone");
  eventWriteNocache =
      createEvent([this](uint64_t, uint64_t d) { write_nocache(d); },
                  "ICL::RingBuffer::eventWriteNocache");
}

void RingBuffer::trigger_readWorker() {
  if (!readTriggered) {
    scheduleNow(eventReadWorker);
  }

  readTriggered = true;
}

void RingBuffer::readWorker(uint64_t now) {
  CPU::Function fstat = CPU::initFunction();

  // Pass all read request to FTL
  std::vector<LPN> pendingLPNs;
  std::list<LPN> alignedLPN;

  pendingLPNs.reserve(readPendingQueue.size());

  for (auto &iter : readPendingQueue) {
    if (iter.status == CacheStatus::ReadWait) {
      // Collect LPNs to merge
      pendingLPNs.emplace_back(iter.scmd->lpn);
    }
  }

  if (pendingLPNs.size() > 0) {
    // Sort
    std::sort(pendingLPNs.begin(), pendingLPNs.end());

    // Merge
    for (auto &iter : pendingLPNs) {
      LPN aligned = alignToMinPage(iter);

      if (alignedLPN.size() == 0 || alignedLPN.back() != aligned) {
        alignedLPN.emplace_back(aligned);
      }
    }
  }

  // Check prefetch
  if (trigger.triggered()) {
    // Append pages
    LPN last;
    uint32_t limit = prefetchPages / minPages;

    if (alignedLPN.size() > 0) {
      last = alignedLPN.back();
      limit = limit > alignedLPN.size() ? limit - alignedLPN.size() : 0;
    }
    else {
      last = lastReadPendingAddress;
    }

    debugprint(Log::DebugID::ICL_RingBuffer,
               "Prefetch/Read-ahead | Fetch LPN %" PRIx64 "h + %" PRIx64 "h",
               last + minPages, last + limit * minPages);

    for (uint32_t i = 1; i <= limit; i++) {
      alignedLPN.emplace_back(last + i * minPages);
    }
  }

  if (alignedLPN.size() == 0) {
    readTriggered = false;

    return;
  }

  // Update last read address
  lastReadPendingAddress = alignedLPN.back();

  // Check capacity
  readWaitsEviction = alignedLPN.size();

  if (readWaitsEviction + cacheEntry.size() >= maxEntryCount) {
    readTriggered = false;

    trigger_writeWorker();
  }
  else {
    // Mark as submit
    for (auto &ctx : readPendingQueue) {
      if (ctx.status == CacheStatus::ReadWait) {
        ctx.status = CacheStatus::ReadPendingWait;
      }
    }

    // Create entry
    for (auto &lpn : alignedLPN) {
      auto ret = cacheEntry.emplace(lpn, Entry(lpn, minPages, iobits));
      auto &entry = ret.first->second;

      // As we got data from FTL, all SubEntries are valid
      // Mark as read pending
      for (auto &iter : entry.list) {
        iter.valid.set();
        iter.rpending = true;
      }

      // Update clock
      entry.accessedAt = clock;

      if (ret.second) {
        entry.insertedAt = clock;
      }
    }

    // Submit
    readWorkerTag.reserve(alignedLPN.size());

    for (auto &iter : alignedLPN) {
      // Create request
      uint64_t tag = makeCacheCommandTag();

      commandManager->createICLRead(tag, eventReadWorkerDone, iter, minPages,
                                    now);

      debugprint(Log::DebugID::ICL_RingBuffer,
                 "Read  | Internal | LPN %" PRIx64 "h + %" PRIx64 "h", iter,
                 minPages);

      // Store for CPU latency
      readWorkerTag.emplace_back(tag);
    }
  }

  scheduleFunction(CPU::CPUGroup::InternalCache, eventReadWorkerDoFTL, fstat);
}

void RingBuffer::readWorker_doFTL() {
  for (auto &iter : readWorkerTag) {
    pFTL->submit(iter);
  }

  readTriggered = false;

  readWorkerTag.clear();
}

void RingBuffer::readWorker_done(uint64_t now, uint64_t tag) {
  auto &cmd = commandManager->getCommand(tag);

  cmd.counter++;

  if (cmd.counter == cmd.length) {
    cmd.counter = 0;

    // Read done
    auto iter = cacheEntry.find(cmd.offset);
    auto &entry = iter->second;

    debugprint(Log::DebugID::ICL_RingBuffer,
               "Read  | Internal | LPN %" PRIx64 "h + %" PRIx64 "h | %" PRIu64
               " - %" PRIu64 " (%" PRIu64 ")",
               cmd.offset, cmd.length, cmd.beginAt, now, now - cmd.beginAt);

    // Mark as read done
    for (auto &iter : entry.list) {
      iter.rpending = false;
    }

    // Apply NVM -> DRAM latency (No completion handler)
    object.dram->write(getDRAMAddress(entry.offset), minPages * pageSize,
                       InvalidEventID);

    // Update capacity
    if (LIKELY(enabled)) {
      // We need to erase some entry
      if (cacheEntry.size() >= maxEntryCount) {
        trigger_writeWorker();
      }
    }

    // Update address
    lastReadDoneAddress = cmd.offset + cmd.length;

    // Handle completion of pending request
    for (auto iter = readPendingQueue.begin();
         iter != readPendingQueue.end();) {
      if (iter->status == CacheStatus::ReadPendingWait &&
          iter->scmd->lpn >= cmd.offset &&
          iter->scmd->lpn < cmd.offset + cmd.length) {
        // This pending read is completed
        iter->scmd->status = Status::Done;

        // Apply DRAM -> PCIe latency (Completion on RingBuffer::read_done)
        object.dram->read(getDRAMAddress(iter->scmd->lpn), pageSize,
                          eventReadDRAMDone, iter->scmd->tag);

        // Done!
        iter = readPendingQueue.erase(iter);
      }
      else {
        ++iter;
      }
    }

    // Destroy
    commandManager->destroyCommand(tag);
  }
}

void RingBuffer::trigger_writeWorker() {
  if ((float)cacheEntry.size() / maxEntryCount >= triggerThreshold &&
      !writeTriggered) {
    writeTriggered = true;

    scheduleNow(eventWriteWorker);
  }
}

void RingBuffer::writeWorker(uint64_t now) {
  CPU::Function fstat = CPU::initFunction();
  /*
   * Some FTL may require 'minPage'-sized write request to prevent
   * read-modify-write operation. So, in this function, find the full-sized
   * request (entry >= minPage) in specified eviction policy. If there was no
   * full-sized request, just evict selected entry (makes read-modify-write).
   */

  // Do we need to write?
  if ((float)dirtyEntryCount / maxEntryCount >= triggerThreshold) {
    // Iterate evictPages / minPages times
    uint32_t times = evictPages / minPages;

    for (uint32_t i = 0; i < times; i++) {
      bool notfound = true;
      auto iter = cacheEntry.end();

      if (!noPageLimit) {
        // Collect full-sized request when FTL wants minPages write
        iter = chooseEntry(SelectionMode::FullSized);

        if (LIKELY(iter != cacheEntry.end())) {
          notfound = false;
        }
      }

      if (notfound) {
        iter = chooseEntry(SelectionMode::All);

        panic_if(iter == cacheEntry.end(), "Not possible case. Bug?");
      }

      if (iter != cacheEntry.end()) {
        fstat += writeWorker_collect(now, iter);
      }
    }
  }

  // Do we have to flush
  for (auto &lpn : flushWorkerLPN) {
    auto iter = cacheEntry.find(lpn);

    panic_if(iter == cacheEntry.end(), "Flushing non-exist entry.");

    fstat += writeWorker_collect(now, iter);
  }

  // Do we need to erase clean entries
  if (readWaitsEviction + cacheEntry.size() >= maxEntryCount) {
    // Just erase some clean entries
    while (readWaitsEviction + cacheEntry.size() >= maxEntryCount) {
      auto iter = chooseEntry(SelectionMode::Clean);

      if (UNLIKELY(iter == cacheEntry.end())) {
        // All entry is write pending
        break;
      }

      cacheEntry.erase(iter);
    }

    // We will not call writeWorker_done
    readWaitsEviction = 0;

    trigger_readWorker();
  }

  scheduleFunction(CPU::CPUGroup::InternalCache, eventWriteWorkerDoFTL, fstat);
}

CPU::Function RingBuffer::writeWorker_collect(uint64_t now,
                                              CacheEntry::iterator iter) {
  CPU::Function fstat = CPU::initFunction();

  panic_if(!isDirty(iter->second.list), "Try to write clean entry.");

  dirtyEntryCount--;

  // Create command(s)
  LPN offset = 0;
  uint32_t length = 0;

  while (iter->second.offset + offset + length <
         iter->second.offset + minPages) {
    // Partial pages are handled in FTL
    if (iter->second.list.at(offset).valid.none() ||
        iter->second.list.at(offset).wpending) {
      offset++;
    }
    else if (iter->second.list.at(offset + length).valid.any() &&
             !iter->second.list.at(offset + length).wpending) {
      length++;
    }
    else {
      uint64_t tag = makeCacheCommandTag();

      commandManager->createICLWrite(
          tag, eventWriteWorkerDone, iter->second.offset + offset, length,
          iter->second.list.at(offset).valid.clz() * minIO,
          iter->second.list.at(offset + length - 1).valid.ctz() * minIO, now);

      debugprint(Log::DebugID::ICL_RingBuffer,
                 "Write | Internal | LPN %" PRIx64 "h + %" PRIx64 "h",
                 iter->second.offset + offset, length);

      writeWorkerTag.emplace_back(tag);

      offset += length;
      length = 0;
    }
  }

  // Last chunk
  if (length > 0) {
    uint64_t tag = makeCacheCommandTag();

    commandManager->createICLWrite(
        tag, eventWriteWorkerDone, iter->second.offset + offset, length,
        iter->second.list.at(offset).valid.clz() * minIO,
        iter->second.list.at(offset + length - 1).valid.ctz() * minIO, now);

    debugprint(Log::DebugID::ICL_RingBuffer,
               "Write | Internal | LPN %" PRIx64 "h + %" PRIx64 "h",
               iter->second.offset + offset, length);

    writeWorkerTag.emplace_back(tag);
  }

  // Mark as write pending and clean (to prevent double eviction)
  for (auto &sentry : iter->second.list) {
    if (sentry.valid.any()) {
      sentry.dirty = false;
      sentry.wpending = true;
    }
  }

  return std::move(fstat);
}

void RingBuffer::writeWorker_doFTL() {
  // Issue writes in writePendingQueue
  for (auto &iter : writeWorkerTag) {
    pFTL->submit(iter);
  }

  writeTriggered = false;

  writeWorkerTag.clear();
}

void RingBuffer::writeWorker_done(uint64_t now, uint64_t tag) {
  auto &cmd = commandManager->getCommand(tag);

  cmd.counter++;

  if (cmd.counter == cmd.length) {
    cmd.counter = 0;

    // Find corresponding entry
    LPN aligned = alignToMinPage(cmd.offset);

    auto iter = cacheEntry.find(aligned);

    panic_if(iter == cacheEntry.end(), "Evicted entry not found.");

    debugprint(Log::DebugID::ICL_RingBuffer,
               "Write | Internal | LPN %" PRIx64 "h + %" PRIx64 "h | %" PRIu64
               " - %" PRIu64 " (%" PRIu64 ")",
               cmd.offset, cmd.length, cmd.beginAt, now, now - cmd.beginAt);

    uint32_t i = cmd.offset - iter->second.offset;
    uint32_t limit = cmd.length + i;

    for (; i < limit; i++) {
      auto &sentry = iter->second.list.at(i);

      sentry.wpending = false;
    }

    commandManager->destroyCommand(tag);

    // Flush?
    if (UNLIKELY(flushWorkerLPN.size() > 0)) {
      auto fiter = flushWorkerLPN.find(aligned);

      if (fiter != flushWorkerLPN.end()) {
        // This request was flush
        flushWorkerLPN.erase(fiter);

        // Complete?
        if (flushWorkerLPN.size() == 0) {
          scheduleNow(flushTag.first, flushTag.second);

          flushTag.first = InvalidEventID;

          debugprint(Log::DebugID::ICL_RingBuffer,
                     "Flush | %" PRIu64 " - %" PRIu64 " (%" PRIu64 ")",
                     flushBeginAt, now, now - flushBeginAt);
        }
      }
    }

    // Retry requests in writeWaitingQueue
    std::vector<SubCommand *> list;

    list.reserve(writeWaitingQueue.size());

    for (auto &iter : writeWaitingQueue) {
      list.emplace_back(iter.scmd);
    }

    writeWaitingQueue.clear();

    for (auto &iter : list) {
      write_find(*iter);
    }

    // Schedule next worker
    trigger_writeWorker();
  }
}

void RingBuffer::read_find(Command &cmd) {
  CPU::Function fstat = CPU::initFunction();
  uint64_t size = cmd.length * pageSize - cmd.subCommandList.front().skipFront -
                  cmd.subCommandList.back().skipEnd;

  stat.request[0] += size;

  debugprint(Log::DebugID::ICL_RingBuffer,
             "Read  | LPN %" PRIx64 "h + %" PRIx64 "h", cmd.offset, cmd.length);

  if (LIKELY(enabled)) {
    // Update prefetch trigger
    if (prefetchEnabled) {
      trigger.update(
          cmd.offset * pageSize + cmd.subCommandList.front().skipFront, size);
    }

    // Find entry including range
    LPN alignedBegin = alignToMinPage(cmd.offset);
    LPN alignedEnd = alignedBegin + DIVCEIL(cmd.length, minPages) * minPages;

    for (LPN lpn = alignedBegin; lpn < alignedEnd; lpn += minPages) {
      auto iter = cacheEntry.find(lpn);

      if (iter != cacheEntry.end()) {
        // Accessed
        iter->second.accessedAt = clock;

        // Mark as done
        for (auto &scmd : cmd.subCommandList) {
          if (iter->second.offset <= scmd.lpn &&
              scmd.lpn < iter->second.offset + minPages) {
            auto &sentry = iter->second.list.at(scmd.lpn - iter->second.offset);

            // Skip checking
            if (!skipCheck(sentry.valid, scmd.skipFront, scmd.skipEnd)) {
              continue;
            }

            if (sentry.rpending) {
              scmd.status = Status::InternalCache;

              debugprint(Log::DebugID::ICL_RingBuffer,
                         "Read  | LPN %" PRIx64 "h | Cache hit, pending read",
                         scmd.lpn);
            }
            else {
              scmd.status = Status::InternalCacheDone;

              debugprint(Log::DebugID::ICL_RingBuffer,
                         "Read  | LPN %" PRIx64 "h | Cache hit", scmd.lpn);
            }

            updateCapacity(true, scmd.skipFront + scmd.skipEnd);
          }
        }
      }
    }

    if (trigger.triggered() &&
        lastReadDoneAddress != std::numeric_limits<uint64_t>::max()) {
      if (cmd.offset < lastReadPendingAddress &&
          (lastReadPendingAddress + minPages) - cmd.offset <=
              prefetchPages * 0.5f) {
        trigger_readWorker();
      }
    }
  }

  // Check we have scmd.status == Submit
  for (auto &scmd : cmd.subCommandList) {
    if (scmd.status == Status::Submit) {
      scmd.status = Status::InternalCache;

      readPendingQueue.emplace_back(
          CacheContext(&scmd, cacheEntry.end(), CacheStatus::ReadWait));

      if (LIKELY(enabled)) {
        debugprint(Log::DebugID::ICL_RingBuffer,
                   "Read  | LPN %" PRIx64 "h | Cache miss", scmd.lpn);
      }

      trigger_readWorker();
    }
    else if (scmd.status == Status::InternalCache) {
      readPendingQueue.emplace_back(
          CacheContext(&scmd, cacheEntry.end(), CacheStatus::ReadPendingWait));
    }
  }

  scheduleFunction(CPU::CPUGroup::InternalCache, eventReadPreCPUDone, cmd.tag,
                   fstat);
}

void RingBuffer::read_findDone(uint64_t tag) {
  auto &cmd = commandManager->getCommand(tag);

  // Apply DRAM -> PCIe latency
  for (auto &scmd : cmd.subCommandList) {
    if (scmd.status == Status::InternalCacheDone) {
      scmd.status = Status::Done;

      object.dram->read(getDRAMAddress(scmd.lpn), pageSize, eventReadDRAMDone,
                        tag);
    }
  }
}

void RingBuffer::read_done(uint64_t tag) {
  auto &cmd = commandManager->getCommand(tag);

  scheduleNow(cmd.eid, tag);
}

void RingBuffer::write_find(SubCommand &scmd) {
  CPU::Function fstat = CPU::initFunction();

  stat.request[1] += pageSize - scmd.skipFront - scmd.skipEnd;

  if (LIKELY(enabled)) {
    // Find entry including scmd
    LPN aligned = alignToMinPage(scmd.lpn);

    auto iter = cacheEntry.find(aligned);

    if (iter != cacheEntry.end()) {
      // Included existing entry
      if (iter->second.offset <= scmd.lpn &&
          scmd.lpn < iter->second.offset + minPages) {
        // Accessed
        iter->second.accessedAt = clock;

        // If this entry was clean, we need to increase dirtyEntryCount
        if (isClean(iter->second.list)) {
          dirtyEntryCount++;
        }

        // Get sub entry
        auto &sentry = iter->second.list.at(scmd.lpn - iter->second.offset);

        // Check write pending
        if (sentry.wpending) {
          scmd.status = Status::InternalCache;

          writeWaitingQueue.emplace_back(
              CacheContext(&scmd, iter, CacheStatus::WriteCacheWait));

          debugprint(Log::DebugID::ICL_RingBuffer,
                     "Write | LPN %" PRIx64 "h | Cache hit, pending write",
                     scmd.lpn);
        }
        else {
          // Mark as done
          scmd.status = Status::InternalCacheDone;

          debugprint(Log::DebugID::ICL_RingBuffer,
                     "Write | LPN %" PRIx64 "h | Cache hit", scmd.lpn);

          // Update entry
          sentry.dirty = true;

          updateSkip(sentry.valid, scmd.skipFront, scmd.skipEnd);
          updateCapacity(false, scmd.skipFront + scmd.skipEnd);
        }
      }
    }
    else {
      if (cacheEntry.size() < maxEntryCount) {
        // In this case, there was no entry for this sub command
        LPN aligned = alignToMinPage(scmd.lpn);

        auto ret =
            cacheEntry.emplace(aligned, Entry(aligned, minPages, iobits));
        auto &entry = ret.first->second;

        entry.accessedAt = clock;
        entry.insertedAt = clock;

        auto &sentry = entry.list.at(scmd.lpn - aligned);

        sentry.dirty = true;

        updateSkip(sentry.valid, scmd.skipFront, scmd.skipEnd);
        updateCapacity(false, scmd.skipFront + scmd.skipEnd);

        // New dirty entry
        dirtyEntryCount++;

        // Mark as done
        scmd.status = Status::InternalCacheDone;

        debugprint(Log::DebugID::ICL_RingBuffer,
                   "Write | LPN %" PRIx64 "h | Cache cold miss", scmd.lpn);
      }
      else {
        scmd.status = Status::InternalCache;

        writeWaitingQueue.emplace_back(
            CacheContext(&scmd, cacheEntry.end(), CacheStatus::WriteCacheWait));

        debugprint(Log::DebugID::ICL_RingBuffer,
                   "Write | LPN %" PRIx64 "h | Cache capacity miss", scmd.lpn);

        trigger_writeWorker();
      }
    }
  }
  else {
    // If cache is disabled, try to make Command as one ICL write command.
    auto &wcmd = commandManager->getCommand(scmd.tag);

    if (scmd.id == 0) {
      // Create minPages aligned requests
      LPN alignedBegin = alignToMinPage(wcmd.offset);
      LPN alignedEnd = alignedBegin + DIVCEIL(wcmd.length, minPages) * minPages;
      LPN front = wcmd.offset - alignedBegin;
      LPN back = alignedEnd - alignedBegin - wcmd.length - front;

      for (LPN i = alignedBegin; i < alignedEnd; i += minPages) {
        LPN offset = alignedBegin;
        LPN length = minPages;
        uint32_t skipFront = 0;
        uint32_t skipEnd = 0;

        if (i == alignedBegin) {
          offset = wcmd.offset;
          length -= front;
          skipFront = wcmd.subCommandList.front().skipFront;
        }
        if (i + minPages == alignedEnd) {
          length -= back;
          skipEnd = wcmd.subCommandList.back().skipEnd;
        }

        uint64_t tag = makeCacheCommandTag();

        commandManager->createICLWrite(tag, eventWriteNocache, offset, length,
                                       skipFront, skipEnd, getTick());

        for (LPN j = offset; j < offset + length; j++) {
          writeWaitingQueue.emplace_back(
              CacheContext(&wcmd.subCommandList.at(j - offset),
                           cacheEntry.end(), CacheStatus::WriteCacheWait));
        }

        writeWorkerTag.emplace_back(tag);
      }
    }

    if (wcmd.length == scmd.id + 1) {
      scheduleNow(eventWriteWorkerDoFTL);
    }
  }

  scheduleFunction(CPU::CPUGroup::InternalCache, eventWritePreCPUDone, scmd.tag,
                   fstat);
}

void RingBuffer::write_findDone(uint64_t tag) {
  auto &cmd = commandManager->getCommand(tag);

  // Apply PCIe -> DRAM latency
  for (auto &scmd : cmd.subCommandList) {
    if (scmd.status == Status::InternalCacheDone) {
      scmd.status = Status::Done;

      object.dram->write(getDRAMAddress(scmd.lpn), pageSize, eventWriteDRAMDone,
                         tag);
    }
  }
}

void RingBuffer::write_done(uint64_t tag) {
  auto &cmd = commandManager->getCommand(tag);

  scheduleNow(cmd.eid, tag);

  trigger_writeWorker();
}

void RingBuffer::write_nocache(uint64_t tag) {
  auto &cmd = commandManager->getCommand(tag);

  for (auto iter = writeWaitingQueue.begin();
       iter != writeWaitingQueue.end();) {
    if (iter->status == CacheStatus::WriteCacheWait &&
        iter->scmd->lpn >= cmd.offset &&
        iter->scmd->lpn < cmd.offset + cmd.length) {
      iter->scmd->status = Status::Done;

      object.dram->write(getDRAMAddress(iter->scmd->lpn), pageSize,
                         eventWriteDRAMDone, iter->scmd->tag);

      iter = writeWaitingQueue.erase(iter);
    }
    else {
      ++iter;
    }
  }

  commandManager->destroyCommand(tag);
}

void RingBuffer::flush_find(Command &cmd) {
  CPU::Function fstat = CPU::initFunction();

  if (LIKELY(enabled && flushWorkerLPN.size() == 0)) {
    LPN alignedBegin = alignToMinPage(cmd.offset);
    LPN alignedEnd = alignedBegin + DIVCEIL(cmd.length, minPages) * minPages;

    for (LPN lpn = alignedBegin; lpn < alignedEnd; lpn++) {
      auto iter = cacheEntry.find(lpn);

      if (iter != cacheEntry.end()) {
        LPN from = MAX(cmd.offset, lpn);
        LPN to = MIN(cmd.offset + cmd.length, lpn + minPages);
        bool dirty = false;

        for (LPN i = from; i < to; i++) {
          auto &sentry = iter->second.list.at(i - lpn);

          if (sentry.dirty) {
            dirty = true;

            break;
          }
        }

        if (dirty) {
          flushWorkerLPN.emplace(lpn);
        }
      }
    }

    // Request worker to write all dirty pages
    if (flushWorkerLPN.size() > 0) {
      cmd.status = Status::InternalCache;

      flushTag.first = cmd.eid;
      flushTag.second = cmd.tag;

      flushBeginAt = getTick();

      debugprint(Log::DebugID::ICL_RingBuffer,
                 "Flush | LPN %" PRIx64 "h + %" PRIx64 "h", alignedBegin,
                 alignedEnd);

      trigger_writeWorker();

      return;
    }
  }

  // Nothing to flush - no dirty lines!
  cmd.status = Status::Done;

  scheduleFunction(CPU::CPUGroup::InternalCache, cmd.eid, cmd.tag, fstat);
}

void RingBuffer::invalidate_find(Command &cmd) {
  if (LIKELY(enabled)) {
    LPN alignedBegin = alignToMinPage(cmd.offset);
    LPN alignedEnd = alignedBegin + DIVCEIL(cmd.length, minPages) * minPages;

    debugprint(Log::DebugID::ICL_RingBuffer,
               "Format | LPN %" PRIx64 "h + %" PRIx64 "h", alignedBegin,
               alignedEnd);

    for (LPN lpn = alignedBegin; lpn < alignedEnd; lpn++) {
      auto iter = cacheEntry.find(lpn);

      if (iter != cacheEntry.end()) {
        LPN from = MAX(cmd.offset, lpn);
        LPN to = MIN(cmd.offset + cmd.length, lpn + minPages);

        for (LPN i = from; i < to; i++) {
          auto &sentry = iter->second.list.at(i - lpn);

          sentry.valid.reset();
        }
      }
    }
  }

  pFTL->submit(cmd.tag);
}

void RingBuffer::enqueue(uint64_t tag, uint32_t id) {
  auto &cmd = commandManager->getCommand(tag);

  // Increase clock
  clock++;

  // Clear counter
  cmd.counter = 0;

  if (id != std::numeric_limits<uint32_t>::max()) {
    if (LIKELY(cmd.opcode == Operation::Write)) {
      // As we can start write only if PCIe DMA done, write is always partial.
      write_find(cmd.subCommandList.at(id));
    }
    else {
      panic("Unexpected opcode.");
    }
  }
  else {
    switch (cmd.opcode) {
      case Operation::Read:
        read_find(cmd);

        break;
      case Operation::Flush:
        flush_find(cmd);

        break;
      case Operation::Trim:
      case Operation::Format:
        invalidate_find(cmd);

        break;
      default:
        panic("Unexpected opcode.");
    }
  }
}

void RingBuffer::setCache(bool set) {
  enabled = set;

  if (enabled) {
    // Clear all previous entries
    cacheEntry.clear();
  }

  dirtyEntryCount = 0;
}

bool RingBuffer::getCache() {
  return enabled;
}

void RingBuffer::getStatList(std::vector<Stat> &list,
                             std::string prefix) noexcept {
  Stat temp;

  temp.name = prefix + "ring_buffer.read.bytes";
  temp.desc = "Read request volume";
  list.push_back(temp);

  temp.name = prefix + "ring_buffer.read.bytes_from_cache";
  temp.desc = "Read requests that served from cache";
  list.push_back(temp);

  temp.name = prefix + "ring_buffer.write.bytes";
  temp.desc = "Write request volume";
  list.push_back(temp);

  temp.name = prefix + "ring_buffer.write.bytes_from_cache";
  temp.desc = "Write requests that served to cache";
  list.push_back(temp);
}

void RingBuffer::getStatValues(std::vector<double> &values) noexcept {
  values.push_back(stat.request[0]);
  values.push_back(stat.cache[0]);
  values.push_back(stat.request[1]);
  values.push_back(stat.cache[1]);
}

void RingBuffer::resetStatValues() noexcept {
  memset(&stat, 0, sizeof(stat));
}

void RingBuffer::createCheckpoint(std::ostream &out) const noexcept {
  BACKUP_SCALAR(out, maxEntryCount);
  BACKUP_SCALAR(out, dirtyEntryCount);
  BACKUP_SCALAR(out, enabled);
  BACKUP_SCALAR(out, prefetchEnabled);
  BACKUP_SCALAR(out, noPageLimit);
  BACKUP_SCALAR(out, minPages);

  uint64_t size = cacheEntry.size();
  BACKUP_SCALAR(out, size);

  for (auto &iter : cacheEntry) {
    BACKUP_SCALAR(out, iter.first);
    BACKUP_SCALAR(out, iter.second.accessedAt);
    BACKUP_SCALAR(out, iter.second.insertedAt);

    for (auto &sentry : iter.second.list) {
      BACKUP_SCALAR(out, sentry.data);
      sentry.valid.createCheckpoint(out);
    }
  }

  BACKUP_SCALAR(out, trigger.requestCounter);
  BACKUP_SCALAR(out, trigger.requestCapacity);
  BACKUP_SCALAR(out, trigger.lastAddress);
  BACKUP_SCALAR(out, trigger.trigger);
  BACKUP_SCALAR(out, prefetchPages);
  BACKUP_SCALAR(out, triggerThreshold);
  BACKUP_SCALAR(out, evictPages);
  BACKUP_SCALAR(out, clock);
  BACKUP_SCALAR(out, evictPolicy);
  BACKUP_BLOB(out, &stat, sizeof(stat));

  BACKUP_SCALAR(out, readTriggered);
  BACKUP_SCALAR(out, writeTriggered);
  BACKUP_SCALAR(out, readWaitsEviction);
  BACKUP_SCALAR(out, lastReadPendingAddress);
  BACKUP_SCALAR(out, lastReadDoneAddress);
  BACKUP_SCALAR(out, flushBeginAt);

  size = readWorkerTag.size();
  BACKUP_SCALAR(out, size);

  for (auto &iter : readWorkerTag) {
    BACKUP_SCALAR(out, iter);
  }

  size = writeWorkerTag.size();
  BACKUP_SCALAR(out, size);

  for (auto &iter : writeWorkerTag) {
    BACKUP_SCALAR(out, iter);
  }

  size = flushWorkerLPN.size();
  BACKUP_SCALAR(out, size);

  for (auto &iter : flushWorkerLPN) {
    BACKUP_SCALAR(out, iter);
  }

  BACKUP_EVENT(out, flushTag.first);
  BACKUP_SCALAR(out, flushTag.second);

  size = readPendingQueue.size();
  BACKUP_SCALAR(out, size);

  for (auto &iter : readPendingQueue) {
    BACKUP_SCALAR(out, iter.status);
    BACKUP_SCALAR(out, iter.scmd->tag);
    BACKUP_SCALAR(out, iter.scmd->id);

    if (iter.entry != cacheEntry.end()) {
      BACKUP_SCALAR(out, iter.entry->first);
    }
    else {
      BACKUP_SCALAR(out, InvalidLPN);
    }
  }

  size = writeWaitingQueue.size();
  BACKUP_SCALAR(out, size);

  for (auto &iter : writeWaitingQueue) {
    BACKUP_SCALAR(out, iter.status);
    BACKUP_SCALAR(out, iter.scmd->tag);
    BACKUP_SCALAR(out, iter.scmd->id);

    if (iter.entry != cacheEntry.end()) {
      BACKUP_SCALAR(out, iter.entry->first);
    }
    else {
      BACKUP_SCALAR(out, InvalidLPN);
    }
  }

  BACKUP_EVENT(out, eventReadWorker);
  BACKUP_EVENT(out, eventReadWorkerDoFTL);
  BACKUP_EVENT(out, eventReadWorkerDone);
  BACKUP_EVENT(out, eventWriteWorker);
  BACKUP_EVENT(out, eventWriteWorkerDoFTL);
  BACKUP_EVENT(out, eventWriteWorkerDone);
  BACKUP_EVENT(out, eventReadPreCPUDone);
  BACKUP_EVENT(out, eventReadDRAMDone);
  BACKUP_EVENT(out, eventWritePreCPUDone);
  BACKUP_EVENT(out, eventWriteDRAMDone);
  BACKUP_EVENT(out, eventWriteNocache);
}

void RingBuffer::restoreCheckpoint(std::istream &in) noexcept {
  uint64_t tmp64;
  uint32_t tmp32;
  uint8_t tmp8;

  RESTORE_SCALAR(in, tmp64);
  panic_if(tmp64 != maxEntryCount, "Cache size not matched while restore.");

  RESTORE_SCALAR(in, dirtyEntryCount);
  RESTORE_SCALAR(in, enabled);
  RESTORE_SCALAR(in, prefetchEnabled);

  RESTORE_SCALAR(in, tmp8);
  panic_if((bool)tmp8 != noPageLimit, "FTL not matched while restore.");

  RESTORE_SCALAR(in, tmp32);
  panic_if(tmp32 != minPages, "FTL not matched while restore.");

  uint64_t size;
  RESTORE_SCALAR(in, size);

  for (uint64_t i = 0; i < size; i++) {
    LPN offset;

    RESTORE_SCALAR(in, offset);

    auto ret = cacheEntry.emplace(offset, Entry(offset, minPages, iobits));

    RESTORE_SCALAR(in, ret.first->second.accessedAt);
    RESTORE_SCALAR(in, ret.first->second.insertedAt);

    for (auto &sentry : ret.first->second.list) {
      RESTORE_SCALAR(in, sentry.data);
      sentry.valid.restoreCheckpoint(in);
    }
  }

  RESTORE_SCALAR(in, trigger.requestCounter);
  RESTORE_SCALAR(in, trigger.requestCapacity);
  RESTORE_SCALAR(in, trigger.lastAddress);
  RESTORE_SCALAR(in, trigger.trigger);
  RESTORE_SCALAR(in, prefetchPages);
  RESTORE_SCALAR(in, triggerThreshold);
  RESTORE_SCALAR(in, evictPages);
  RESTORE_SCALAR(in, clock);
  RESTORE_SCALAR(in, evictPolicy);
  RESTORE_BLOB(in, &stat, sizeof(stat));

  RESTORE_SCALAR(in, readTriggered);
  RESTORE_SCALAR(in, writeTriggered);
  RESTORE_SCALAR(in, readWaitsEviction);
  RESTORE_SCALAR(in, lastReadPendingAddress);
  RESTORE_SCALAR(in, lastReadDoneAddress);
  RESTORE_SCALAR(in, flushBeginAt);

  RESTORE_SCALAR(in, size);

  readWorkerTag.reserve(size);

  for (uint64_t i = 0; i < size; i++) {
    uint64_t tag;

    RESTORE_SCALAR(in, tag);
    readWorkerTag.emplace_back(tag);
  }

  RESTORE_SCALAR(in, size);

  writeWorkerTag.reserve(size);

  for (uint64_t i = 0; i < size; i++) {
    uint64_t tag;

    RESTORE_SCALAR(in, tag);
    writeWorkerTag.emplace_back(tag);
  }

  RESTORE_SCALAR(in, size);

  writeWorkerTag.reserve(size);

  for (uint64_t i = 0; i < size; i++) {
    uint64_t tag;

    RESTORE_SCALAR(in, tag);
    writeWorkerTag.emplace_back(tag);
  }

  RESTORE_EVENT(in, flushTag.first);
  RESTORE_SCALAR(in, flushTag.second);

  RESTORE_SCALAR(in, size);

  for (uint64_t i = 0; i < size; i++) {
    CacheStatus cs;
    uint64_t tag;
    uint32_t id;
    LPN offset;

    RESTORE_SCALAR(in, cs);
    RESTORE_SCALAR(in, tag);
    RESTORE_SCALAR(in, id);
    RESTORE_SCALAR(in, offset);

    auto &scmd = commandManager->getSubCommand(tag).at(id);

    if (offset == InvalidLPN) {
      readPendingQueue.emplace_back(CacheContext(&scmd, cacheEntry.end(), cs));
    }
    else {
      auto iter = cacheEntry.find(offset);

      panic_if(iter == cacheEntry.end(), "Entry not found while restore.");

      readPendingQueue.emplace_back(CacheContext(&scmd, iter, cs));
    }
  }

  RESTORE_SCALAR(in, size);

  for (uint64_t i = 0; i < size; i++) {
    CacheStatus cs;
    uint64_t tag;
    uint32_t id;
    LPN offset;

    RESTORE_SCALAR(in, cs);
    RESTORE_SCALAR(in, tag);
    RESTORE_SCALAR(in, id);
    RESTORE_SCALAR(in, offset);

    auto &scmd = commandManager->getSubCommand(tag).at(id);

    if (offset == std::numeric_limits<LPN>::max()) {
      writeWaitingQueue.emplace_back(CacheContext(&scmd, cacheEntry.end(), cs));
    }
    else {
      auto iter = cacheEntry.find(offset);

      panic_if(iter == cacheEntry.end(), "Entry not found while restore.");

      writeWaitingQueue.emplace_back(CacheContext(&scmd, iter, cs));
    }
  }

  RESTORE_EVENT(in, eventReadWorker);
  RESTORE_EVENT(in, eventReadWorkerDoFTL);
  RESTORE_EVENT(in, eventReadWorkerDone);
  RESTORE_EVENT(in, eventWriteWorker);
  RESTORE_EVENT(in, eventWriteWorkerDoFTL);
  RESTORE_EVENT(in, eventWriteWorkerDone);
  RESTORE_EVENT(in, eventReadPreCPUDone);
  RESTORE_EVENT(in, eventReadDRAMDone);
  RESTORE_EVENT(in, eventWritePreCPUDone);
  RESTORE_EVENT(in, eventWriteDRAMDone);
  RESTORE_EVENT(in, eventWriteNocache);
}

}  // namespace SimpleSSD::ICL
