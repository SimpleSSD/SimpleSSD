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
      writeTriggered(false) {
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

  totalCapacity =
      readConfigUint(Section::InternalCache, Config::Key::CacheSize);

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
            if (!isDirty(iter->second.list)) {
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
              if (isDirty(iter->second.list)) {
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
              if (isDirty(iter->second.list)) {
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
  eventReadWorker = createEvent([this](uint64_t, uint64_t) { readWorker(); },
                                "ICL::RingBuffer::eventReadWorker");
  eventReadWorkerDoFTL =
      createEvent([this](uint64_t, uint64_t) { readWorker_doFTL(); },
                  "ICL::RingBuffer::eventReadWorkerDoFTL");
  eventReadWorkerDone =
      createEvent([this](uint64_t, uint64_t d) { readWorker_done(d); },
                  "ICL::RingBuffer::eventReadWorkerDone");
  eventWriteWorker = createEvent([this](uint64_t, uint64_t) { writeWorker(); },
                                 "ICL::RingBuffer::eventWriteWorker");
  eventWriteWorkerDoFTL =
      createEvent([this](uint64_t, uint64_t) { writeWorker_doFTL(); },
                  "ICL::RingBuffer::eventWriteWorkerDoFTL");
  eventWriteWorkerDone =
      createEvent([this](uint64_t, uint64_t d) { writeWorker_done(d); },
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
}

void RingBuffer::trigger_readWorker() {
  if (!readTriggered) {
    scheduleNow(eventReadWorker);
  }

  readTriggered = true;
}

void RingBuffer::readWorker() {
  CPU::Function fstat = CPU::initFunction();

  // Pass all read request to FTL
  std::vector<LPN> pendingLPNs;

  pendingLPNs.reserve(readPendingQueue.size());

  for (auto &iter : readPendingQueue) {
    if (iter.status == CacheStatus::ReadWait) {
      // Collect LPNs to merge
      pendingLPNs.emplace_back(iter.scmd->lpn);
    }
  }

  // Check prefetch
  if (trigger.triggered()) {
    // Append pages
    LPN last;

    if (pendingLPNs.size() > 0) {
      last = pendingLPNs.back();
    }
    else {
      last = lastReadAddress;
    }

    for (uint32_t i = minPages; i <= prefetchPages; i += minPages) {
      pendingLPNs.emplace_back(last + i);
    }
  }

  if (UNLIKELY(pendingLPNs.size() == 0)) {
    readTriggered = false;

    return;
  }

  // Sort
  std::sort(pendingLPNs.begin(), pendingLPNs.end());

  // Merge
  std::list<LPN> alignedLPN;

  for (auto &iter : pendingLPNs) {
    LPN aligned = alignToMinPage(iter);

    if (alignedLPN.size() == 0 || alignedLPN.back() != aligned) {
      alignedLPN.emplace_back(aligned);
    }
  }

#ifndef EXCLUDE_CPU
  // Maybe collected LPN is already in read O(n^2)
  for (auto &ctx : readPendingQueue) {
    if (ctx.status == CacheStatus::FTL) {
      LPN aligned = alignToMinPage(ctx.scmd->lpn);

      for (auto iter = alignedLPN.begin(); iter != alignedLPN.end();) {
        if (*iter == aligned) {
          iter = alignedLPN.erase(iter);
        }
        else {
          ++iter;
        }
      }
    }
  }

  if (UNLIKELY(alignedLPN.size() == 0)) {
    readTriggered = false;
    return;
  }
#endif

  // Update last read address
  lastReadAddress = alignedLPN.back();

  // Check capacity
  readWaitsEviction = alignedLPN.size() * pageSize;

  if (readWaitsEviction + usedCapacity >= totalCapacity) {
    readTriggered = false;

    trigger_writeWorker();
  }
  else {
    // Mark as submit
    for (auto &ctx : readPendingQueue) {
      if (ctx.status == CacheStatus::ReadWait) {
        ctx.status = CacheStatus::FTL;
      }
    }

    // Submit
    readWorkerTag.reserve(alignedLPN.size());

    for (auto &iter : alignedLPN) {
      // Create request
      uint64_t tag = makeCacheCommandTag();

      commandManager->createICLRead(tag, eventReadWorkerDone, iter, minPages);

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

void RingBuffer::readWorker_done(uint64_t tag) {
  auto &cmd = commandManager->getCommand(tag);

  cmd.counter++;

  if (cmd.counter == cmd.length) {
    cmd.counter = 0;

    // Read done, prepare new entry
    auto ret =
        cacheEntry.emplace(cmd.offset, Entry(cmd.offset, minPages, iobits));
    auto &entry = ret.first->second;

    // Update clock
    entry.accessedAt = clock;

    if (ret.second) {
      entry.insertedAt = clock;
    }

    // As we got data from FTL, all SubEntries are valid
    for (auto &iter : entry.list) {
      iter.valid.set();
    }

    // Apply NVM -> DRAM latency (No completion handler)
    object.dram->write(getDRAMAddress(entry.offset), minPages * pageSize,
                       InvalidEventID);

    // Update capacity
    if (LIKELY(enabled)) {
      usedCapacity += cmd.length * pageSize;

      // We need to erase some entry
      if (usedCapacity >= totalCapacity) {
        trigger_writeWorker();
      }
    }

    // Handle completion of pending request
    for (auto iter = readPendingQueue.begin();
         iter != readPendingQueue.end();) {
      if (iter->status == CacheStatus::FTL && iter->scmd->lpn >= cmd.offset &&
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
  if ((float)usedCapacity / totalCapacity >= triggerThreshold &&
      !writeTriggered) {
    writeTriggered = true;

    scheduleNow(eventWriteWorker);
  }
}

void RingBuffer::writeWorker() {
  CPU::Function fstat = CPU::initFunction();
  /*
   * Some FTL may require 'minPage'-sized write request to prevent
   * read-modify-write operation. So, in this function, find the full-sized
   * request (entry >= minPage) in specified eviction policy. If there was no
   * full-sized request, just evict selected entry (makes read-modify-write).
   */

  // Do we need to write?
  if ((float)dirtyCapacity / totalCapacity >= triggerThreshold) {
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

      panic_if(iter == cacheEntry.end(),
               "Why write worker is flushing entries?");
    }

    if (iter != cacheEntry.end()) {
      // Mark as write pending
      for (auto &sentry : iter->second.list) {
        if (sentry.valid.any()) {
          sentry.wpending = true;
        }
      }

      // Create command(s)
      LPN offset = 0;
      uint32_t length = 0;

      while (offset + length < iter->second.offset + minPages) {
        // TODO: Consider partial pages
        if (iter->second.list.at(offset).valid.none()) {
          offset++;
        }
        else if (iter->second.list.at(offset + length).valid.any()) {
          length++;
        }
        else {
          uint64_t tag = makeCacheCommandTag();

          commandManager->createICLWrite(tag, eventWriteWorkerDone, offset,
                                         length);

          offset += length;
          length = 0;

          writeWorkerTag.emplace_back(tag);
        }
      }
    }
  }
  else {
    // Just erase some clean entries
    while (readWaitsEviction + usedCapacity >= totalCapacity) {
      auto iter = chooseEntry(SelectionMode::Clean);

      panic_if(iter == cacheEntry.end(), "Not possible case. Bug?");

      usedCapacity -= minPages * pageSize;

      cacheEntry.erase(iter);
    }

    // We will not call writeWorker_done
    writeTriggered = false;

    readWaitsEviction = 0;
    trigger_readWorker();

    return;
  }

  scheduleFunction(CPU::CPUGroup::InternalCache, eventWriteWorkerDoFTL, fstat);
}

void RingBuffer::writeWorker_doFTL() {
  // Issue writes in writePendingQueue
  for (auto &iter : writeWorkerTag) {
    pFTL->submit(iter);
  }

  writeWorkerTag.clear();
}

void RingBuffer::writeWorker_done(uint64_t tag) {
  auto &cmd = commandManager->getCommand(tag);

  cmd.counter++;

  if (cmd.counter == cmd.length) {
    cmd.counter = 0;

    // Find corresponding entry
    for (auto &iter : cacheEntry) {
      if (iter.second.offset <= cmd.offset &&
          cmd.offset + cmd.length <= iter.second.offset + minPages) {
        uint32_t i = cmd.offset - iter.second.offset;
        uint32_t limit = cmd.length + i;

        for (; i < limit; i++) {
          auto &sentry = iter.second.list.at(i);

          sentry.dirty = false;
          sentry.wpending = false;
        }

        dirtyCapacity -= cmd.length * pageSize;
        usedCapacity -= cmd.length * pageSize;
      }
    }

    commandManager->destroyCommand(tag);

    // Flush?
    if (UNLIKELY(dirtyCapacity == 0 && flushEvents.size() > 0)) {
      for (auto &iter : flushEvents) {
        auto &fcmd = commandManager->getCommand(iter);

        scheduleNow(fcmd.eid, iter);
      }

      flushEvents.clear();
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
    writeTriggered = false;

    trigger_writeWorker();
  }
}

void RingBuffer::read_find(Command &cmd) {
  CPU::Function fstat = CPU::initFunction();
  uint64_t size = cmd.length * pageSize - cmd.subCommandList.front().skipFront -
                  cmd.subCommandList.back().skipEnd;

  stat.request[0] += size;

  if (LIKELY(enabled)) {
    // Update prefetch trigger
    if (prefetchEnabled) {
      trigger.update(
          cmd.offset * pageSize + cmd.subCommandList.front().skipFront, size);
    }

    // Find entry including range
    for (auto iter = cacheEntry.begin(); iter != cacheEntry.end(); ++iter) {
      uint8_t bound = 0;

      if (iter->second.offset <= cmd.offset &&
          cmd.offset < iter->second.offset + minPages) {
        bound |= 1;
      }
      if (iter->second.offset < cmd.offset + cmd.length &&
          cmd.offset + cmd.length <= iter->second.offset + minPages) {
        bound |= 2;
      }

      if (bound != 0) {
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

            scmd.status = Status::InternalCacheDone;

            updateCapacity(true, scmd.skipFront + scmd.skipEnd);

            cmd.counter++;
          }
        }
      }

      if (cmd.counter == cmd.subCommandList.size()) {
        break;
      }
    }

    cmd.counter = 0;

    if (trigger.triggered() &&
        lastReadAddress != std::numeric_limits<uint64_t>::max()) {
      // Always lastaddress >= offset
      if (lastReadAddress - cmd.offset < prefetchPages / 2) {
        trigger_readWorker();
      }
    }
    else {
      lastReadAddress = std::numeric_limits<uint64_t>::max();
    }
  }

  // Check we have scmd.status == Submit
  for (auto &scmd : cmd.subCommandList) {
    if (scmd.status == Status::Submit) {
      scmd.status = Status::InternalCache;

      readPendingQueue.emplace_back(
          CacheContext(&scmd, cacheEntry.end(), CacheStatus::ReadWait));

      trigger_readWorker();
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
    for (auto iter = cacheEntry.begin(); iter != cacheEntry.end(); ++iter) {
      // Included existing entry
      if (iter->second.offset <= scmd.lpn &&
          scmd.lpn < iter->second.offset + minPages) {
        // Accessed
        iter->second.accessedAt = clock;

        // Get sub entry
        auto &sentry = iter->second.list.at(scmd.lpn - iter->second.offset);

        // Check write pending
        if (sentry.wpending) {
          writeWaitingQueue.emplace_back(
              CacheContext(&scmd, iter, CacheStatus::WriteCacheWait));
        }
        else {
          // Mark as done
          scmd.status = Status::InternalCacheDone;

          // Update entry
          sentry.dirty = true;

          updateSkip(sentry.valid, scmd.skipFront, scmd.skipEnd);
          updateCapacity(false, scmd.skipFront + scmd.skipEnd);
        }

        break;
      }
    }

    // Check done
    if (scmd.status == Status::Submit) {
      if (usedCapacity + pageSize < totalCapacity) {
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
      }
      else {
        scmd.status = Status::InternalCache;

        writeWaitingQueue.emplace_back(
            CacheContext(&scmd, cacheEntry.end(), CacheStatus::WriteCacheWait));
      }
    }
  }
  else {
    scmd.status = Status::InternalCache;
    uint64_t tag = makeCacheCommandTag();

    auto &wcmd = commandManager->getCommand(scmd.tag);

    commandManager->createICLWrite(tag, wcmd.eid, scmd.lpn, 1);

    writeWorkerTag.emplace_back(tag);

    scheduleNow(eventWriteWorkerDoFTL);
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

void RingBuffer::flush_find(Command &cmd) {
  if (LIKELY(enabled)) {
    bool dirty = false;

    // Find dirty entry
    for (auto &iter : cacheEntry) {
      if (isDirty(iter.second.list)) {
        dirty = true;

        break;
      }
    }

    // Request worker to write all dirty pages
    if (dirty) {
      flushEvents.emplace_back(cmd.tag);

      trigger_writeWorker();
    }
  }
  else {
    // Nothing to flush - no dirty lines!
    cmd.status = Status::Done;
  }
}

void RingBuffer::invalidate_find(Command &cmd) {
  if (LIKELY(enabled)) {
    for (auto iter = cacheEntry.begin(); iter != cacheEntry.end(); ++iter) {
      uint8_t bound = 0;

      if (iter->second.offset <= cmd.offset &&
          cmd.offset < iter->second.offset + minPages) {
        bound |= 1;
      }
      if (iter->second.offset < cmd.offset + cmd.length &&
          cmd.offset + cmd.length <= iter->second.offset + minPages) {
        bound |= 2;
      }

      if (bound != 0) {
        LPN from = MAX(cmd.offset, iter->second.offset);
        LPN to = from + MIN(cmd.length, minPages);

        for (LPN lpn = from; lpn < to; lpn++) {
          auto &sentry = iter->second.list.at(lpn - from);

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

  usedCapacity = 0;
  dirtyCapacity = 0;
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
  BACKUP_SCALAR(out, totalCapacity);
  BACKUP_SCALAR(out, usedCapacity);
  BACKUP_SCALAR(out, dirtyCapacity);
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
  BACKUP_SCALAR(out, lastReadAddress);

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

  size = flushEvents.size();
  BACKUP_SCALAR(out, size);

  for (auto &iter : flushEvents) {
    BACKUP_SCALAR(out, iter);
  }

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
}

void RingBuffer::restoreCheckpoint(std::istream &in) noexcept {
  uint64_t tmp64;
  uint32_t tmp32;
  uint8_t tmp8;

  RESTORE_SCALAR(in, tmp64);
  panic_if(tmp64 != totalCapacity, "Cache size not matched while restore.");

  RESTORE_SCALAR(in, usedCapacity);
  RESTORE_SCALAR(in, dirtyCapacity);
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
  RESTORE_SCALAR(in, lastReadAddress);

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

  flushEvents.reserve(size);

  for (uint64_t i = 0; i < size; i++) {
    uint64_t tag;

    RESTORE_SCALAR(in, tag);
    flushEvents.emplace_back(tag);
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

    if (offset == InvalidLPN) {
      readPendingQueue.emplace_back(CacheContext(&scmd, cacheEntry.end(), cs));
    }
    else {
      for (auto iter = cacheEntry.begin(); iter != cacheEntry.end(); ++iter) {
        if (iter->first == offset) {
          readPendingQueue.emplace_back(CacheContext(&scmd, iter, cs));

          break;
        }
      }
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
      for (auto iter = cacheEntry.begin(); iter != cacheEntry.end(); ++iter) {
        if (iter->first == offset) {
          writeWaitingQueue.emplace_back(CacheContext(&scmd, iter, cs));

          break;
        }
      }
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
}

}  // namespace SimpleSSD::ICL
