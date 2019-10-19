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
      lastRequestID(std::numeric_limits<uint64_t>::max()),
      requestCounter(0),
      requestCapacity(0),
      lastAddress(0),
      trigger(false) {}

void RingBuffer::PrefetchTrigger::update(uint64_t tag, LPN addr,
                                         uint32_t size) {
  // New request arrived, check sequential
  if (addr == lastAddress + 1) {
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

  trigger = false;
}

bool RingBuffer::PrefetchTrigger::triggered() {
  return trigger;
}

RingBuffer::RingBuffer(ObjectData &o, HIL::CommandManager *m, FTL::FTL *p)
    : AbstractCache(o, m, p),
      pageSize(p->getInfo()->pageSize),
      iobits(pageSize / minIO),
      trigger(
          readConfigUint(Section::InternalCache, Config::Key::PrefetchCount),
          readConfigUint(Section::InternalCache, Config::Key::PrefetchRatio)),
      readTriggered(false),
      writeTriggered(false) {
  auto param = pFTL->getInfo();
  uint8_t limit = 0;

  triggerThreshold =
      readConfigFloat(Section::InternalCache, Config::Key::EvictThreshold);

  // Make granularity
  evictPages = 1;

  switch ((Config::Granularity)readConfigUint(Section::InternalCache,
                                              Config::Key::EvictMode)) {
    case Config::Granularity::SuperpageLevel:
      limit = param->superpageLevel;

      break;
    case Config::Granularity::AllLevel:
      limit = 4;

      break;
    default:
      panic("Invalid eviction granularity.");

      break;
  }

  for (uint8_t i = 0; i < limit; i++) {
    evictPages *= param->parallelismLevel[i];
  }

  // Calculate FTL's write granularity
  minPages = 1;

  for (uint8_t i = 0; i < param->superpageLevel; i++) {
    minPages *= param->parallelismLevel[i];
  }

  if (minPages == 1) {
    noPageLimit = true;

    minPages = param->parallelismLevel[0];  // First parallelism level
  }
  else {
    noPageLimit = false;
  }

  prefetchPages = 1;

  switch ((Config::Granularity)readConfigUint(Section::InternalCache,
                                              Config::Key::PrefetchMode)) {
    case Config::Granularity::SuperpageLevel:
      limit = param->superpageLevel;

      break;
    case Config::Granularity::AllLevel:
      limit = 4;

      break;
    default:
      panic("Invalid prefetch granularity.");

      break;
  }

  for (uint8_t i = 0; i < limit; i++) {
    prefetchPages *= param->parallelismLevel[i];
  }

  enabled = readConfigBoolean(Section::InternalCache, Config::Key::EnableCache);
  prefetchEnabled =
      readConfigBoolean(Section::InternalCache, Config::Key::EnablePrefetch);

  totalCapacity =
      readConfigUint(Section::InternalCache, Config::Key::CacheSize);

  setCache(enabled);

  debugprint(Log::DebugID::ICL_SetAssociative, "CREATE  | Capacity %" PRIu64,
             totalCapacity);
  debugprint(Log::DebugID::ICL_SetAssociative,
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
        std::vector<std::list<Entry>::iterator> list;

        if (sel == SelectionMode::All) {
          for (auto iter = cacheEntry.begin(); iter != cacheEntry.end();
               ++iter) {
            if (isDirty(iter->list)) {
              list.emplace_back(iter);
            }
          }

          std::uniform_int_distribution<uint32_t> dist(0, list.size() - 1);

          return list.at(dist(mtengine));
        }
        else if (sel == SelectionMode::FullSized) {
          for (auto iter = cacheEntry.begin(); iter != cacheEntry.end();
               ++iter) {
            if (isFullSizeDirty(iter->list)) {
              list.emplace_back(iter);
            }
          }

          std::uniform_int_distribution<uint32_t> dist(0, list.size() - 1);

          return list.at(dist(mtengine));
        }
        else {
          for (auto iter = cacheEntry.begin(); iter != cacheEntry.end();
               ++iter) {
            if (!isDirty(iter->list)) {
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
        if (sel == SelectionMode::All) {
          auto iter = cacheEntry.begin();

          for (; iter != cacheEntry.end(); iter++) {
            if (isDirty(iter->list)) {
              break;
            }
          }

          return iter;
        }
        else if (sel == SelectionMode::FullSized) {
          auto iter = cacheEntry.begin();

          for (; iter != cacheEntry.end(); iter++) {
            if (isFullSizeDirty(iter->list)) {
              break;
            }
          }

          return iter;
        }
        else {
          auto iter = cacheEntry.begin();

          for (; iter != cacheEntry.end(); iter++) {
            if (!isDirty(iter->list)) {
              break;
            }
          }

          return iter;
        }
      };

      break;
    case Config::EvictModeType::LRU:
      chooseEntry = [this](SelectionMode sel) {
        uint16_t diff = 0;
        auto iter = cacheEntry.end();

        // Find line with largest difference
        for (auto i = cacheEntry.begin(); i != cacheEntry.end(); ++i) {
          if (diff > clock - i->accessedAt) {
            if (sel == SelectionMode::All) {
              if (!isDirty(iter->list)) {
                continue;
              }
            }
            else if (sel == SelectionMode::FullSized) {
              if (!isFullSizeDirty(iter->list)) {
                continue;
              }
            }
            else {
              if (isDirty(iter->list)) {
                continue;
              }
            }

            diff = clock - i->accessedAt;
            iter = i;
          }
        }

        return iter;
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

  for (auto &iter : readPendingQueue) {
    if (iter.status == CacheStatus::ReadWait) {
      // Collect LPNs to merge
      pendingLPNs.emplace_back(iter.scmd->lpn);
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

    if (alignedLPN.back() != aligned) {
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
    else {
      ctx.status = CacheStatus::FTL;
    }
  }

  if (UNLIKELY(alignedLPN.size() == 0)) {
    readTriggered = false;
    return;
  }
#endif

  // Check prefetch
  if (trigger.triggered()) {
    // Append pages
    for (uint32_t i = minPages; i <= prefetchPages; i += minPages) {
      alignedLPN.emplace_back(alignedLPN.back() + i);
    }
  }

  // Submit
  for (auto &iter : alignedLPN) {
    // Create request
    auto &cmd = commandManager->createCommand(makeCacheCommandTag(),
                                              eventReadWorkerDone);
    cmd.opcode = HIL::Operation::Read;
    cmd.offset = iter;
    cmd.length = minPages;

    // Store for CPU latency
    readWorkerTag.emplace_back(cmd.tag);
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

  // Read done, prepare new entry
  Entry entry(cmd.offset, minPages);

  // As we got data from FTL, all SubEntries are valid
  for (auto &iter : entry.list) {
    iter.valid.set();
  }

  // Apply NVM -> DRAM latency (No completion handler)
  object.dram->write(getDRAMAddress(entry.offset), minPages * pageSize,
                     InvalidEventID);

  // Insert
  cacheEntry.emplace_back(entry);

  // Update capacity
  usedCapacity += cmd.length * pageSize;

  // We need to erase some entry
  if (usedCapacity >= totalCapacity) {
    trigger_writeWorker();
  }

  // Handle completion of pending request
  for (auto iter = readPendingQueue.begin(); iter != readPendingQueue.end();) {
    if (iter->status == CacheStatus::FTL && cmd.offset <= iter->scmd->lpn &&
        iter->scmd->lpn < cmd.offset + cmd.length) {
      // This pending read is completed
      iter->scmd->status = HIL::Status::Done;

      // Apply DRAM -> PCIe latency (Completion on RingBuffer::read_done)
      object.dram->read(getDRAMAddress(iter->scmd->lpn), pageSize,
                        eventReadDRAMDone, iter->scmd->tag);

      // Done!
      iter = readPendingQueue.erase(iter);
    }
  }

  // Destroy
  commandManager->destroyCommand(tag);
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
      for (auto &sentry : iter->list) {
        if (sentry.valid.any()) {
          sentry.wpending = true;
        }
      }

      // Create command(s)
      LPN offset = 0;
      uint32_t length = 0;

      while (offset + length < iter->offset + minPages) {
        // TODO: Consider partial pages
        if (iter->list.at(offset).valid.none()) {
          offset++;
        }
        else if (iter->list.at(offset + length).valid.any()) {
          length++;
        }
        else {
          auto &cmd = commandManager->createCommand(makeCacheCommandTag(),
                                                    eventWriteWorkerDone);
          cmd.opcode = HIL::Operation::Write;
          cmd.offset = offset;
          cmd.length = length;

          offset += length;
          length = 0;

          writeWorkerTag.emplace_back(cmd.tag);
        }
      }
    }
  }
  else {
    // Just erase some clean entries
    while (usedCapacity >= totalCapacity) {
      auto iter = chooseEntry(SelectionMode::Clean);

      if (iter != cacheEntry.end()) {
        usedCapacity -= minPages * pageSize;

        cacheEntry.erase(iter);
      }
      else {
        panic("Is this possible?");
      }
    }

    // We will not call writeWorker_done
    writeTriggered = false;

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

  // Find corresponding entry
  for (auto &iter : cacheEntry) {
    if (iter.offset <= cmd.offset &&
        cmd.offset + cmd.length <= iter.offset + minPages) {
      uint32_t i = cmd.offset - iter.offset;
      uint32_t limit = cmd.length + i;

      for (; i < limit; i++) {
        auto &sentry = iter.list.at(i);

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
  std::vector<HIL::SubCommand *> list;

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

void RingBuffer::read_find(HIL::Command &cmd) {
  CPU::Function fstat = CPU::initFunction();

  stat.request[0] += cmd.length * pageSize -
                     cmd.subCommandList.front().skipFront -
                     cmd.subCommandList.back().skipEnd;

  if (LIKELY(enabled)) {
    // Update prefetch trigger
    if (prefetchEnabled) {
      trigger.update(cmd.tag, cmd.offset, cmd.length);
    }

    // Find entry including range
    for (auto iter = cacheEntry.begin(); iter != cacheEntry.end(); ++iter) {
      uint8_t bound = 0;

      if (cmd.offset < iter->offset + minPages) {
        bound |= 1;
      }
      if (iter->offset < cmd.offset + cmd.length) {
        bound |= 2;
      }

      if (bound != 0) {
        // Accessed
        iter->accessedAt = clock;

        // Mark as done
        for (auto &scmd : cmd.subCommandList) {
          if (iter->offset <= scmd.lpn && scmd.lpn < iter->offset + minPages) {
            auto &sentry = iter->list.at(scmd.lpn - iter->offset);

            // Skip checking
            if (!skipCheck(sentry.valid, scmd.skipFront, scmd.skipEnd)) {
              continue;
            }

            scmd.status = HIL::Status::InternalCacheDone;

            updateCapacity(true, scmd.skipFront + scmd.skipEnd);
          }
        }
      }
    }
  }

  // Check we have scmd.status == Submit
  for (auto &scmd : cmd.subCommandList) {
    if (scmd.status == HIL::Status::Submit) {
      scmd.status = HIL::Status::InternalCache;

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
    if (scmd.status == HIL::Status::InternalCacheDone) {
      scmd.status == HIL::Status::Done;

      object.dram->read(getDRAMAddress(scmd.lpn), pageSize, eventReadDRAMDone,
                        tag);
    }
  }
}

void RingBuffer::read_done(uint64_t tag) {
  auto &cmd = commandManager->getCommand(tag);

  scheduleNow(cmd.eid, tag);
}

void RingBuffer::write_find(HIL::SubCommand &scmd) {
  CPU::Function fstat = CPU::initFunction();

  stat.request[1] += pageSize - scmd.skipFront - scmd.skipEnd;

  if (LIKELY(enabled)) {
    // Find entry including scmd
    for (auto iter = cacheEntry.begin(); iter != cacheEntry.end(); ++iter) {
      // Included existing entry
      if (iter->offset <= scmd.lpn && scmd.lpn < iter->offset + minPages) {
        // Accessed
        iter->accessedAt = clock;

        // Get sub entry
        auto &sentry = iter->list.at(scmd.lpn - iter->offset);

        // Check write pending
        if (sentry.wpending) {
          writeWaitingQueue.emplace_back(
              CacheContext(&scmd, iter, CacheStatus::WriteCacheWait));
        }
        else {
          // Mark as done
          scmd.status = HIL::Status::InternalCacheDone;

          // Update entry
          sentry.dirty = true;

          updateSkip(sentry.valid, scmd.skipFront, scmd.skipEnd);
          updateCapacity(false, scmd.skipFront + scmd.skipEnd);
        }

        break;
      }
    }

    // Check done
    if (scmd.status == HIL::Status::Submit) {
      if (usedCapacity + pageSize < totalCapacity) {
        // In this case, there was no entry for this sub command
        LPN aligned = alignToMinPage(scmd.lpn);
        Entry entry(aligned, minPages);

        entry.accessedAt = clock;

        auto &sentry = entry.list.at(scmd.lpn - aligned);

        sentry.dirty = true;

        updateSkip(sentry.valid, scmd.skipFront, scmd.skipEnd);

        // Insert
        cacheEntry.emplace_back(entry);

        updateCapacity(false, scmd.skipFront + scmd.skipEnd);
      }
      else {
        scmd.status = HIL::Status::InternalCache;

        writeWaitingQueue.emplace_back(
            CacheContext(&scmd, cacheEntry.end(), CacheStatus::WriteCacheWait));
      }
    }
  }
  else {
    scmd.status = HIL::Status::InternalCache;

    auto &wcmd = commandManager->getCommand(scmd.tag);
    auto &cmd = commandManager->createCommand(makeCacheCommandTag(), wcmd.eid);

    cmd.opcode = HIL::Operation::Write;
    cmd.offset = scmd.lpn;
    cmd.length = 1;

    writeWorkerTag.emplace_back(cmd.tag);

    scheduleNow(eventWriteWorkerDoFTL);
  }

  scheduleFunction(CPU::CPUGroup::InternalCache, eventWritePreCPUDone, scmd.tag,
                   fstat);
}

void RingBuffer::write_findDone(uint64_t tag) {
  auto &cmd = commandManager->getCommand(tag);

  // Apply PCIe -> DRAM latency
  for (auto &scmd : cmd.subCommandList) {
    if (scmd.status == HIL::Status::InternalCacheDone) {
      scmd.status == HIL::Status::Done;

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

void RingBuffer::flush_find(HIL::Command &cmd) {
  if (LIKELY(enabled)) {
    bool dirty = false;

    // Find dirty entry
    for (auto &iter : cacheEntry) {
      if (isDirty(iter.list)) {
        dirty = true;

        break;
      }
    }

    // Request worker to write all dirty pages
    if (dirty) {
      flushEvents.push_back(cmd.tag);

      trigger_writeWorker();
    }
  }
  else {
    // Nothing to flush - no dirty lines!
    cmd.status = HIL::Status::Done;
  }
}

void RingBuffer::invalidate_find(HIL::Command &cmd) {
  CPU::Function fstat = CPU::initFunction();

  if (LIKELY(enabled)) {
    for (auto iter = cacheEntry.begin(); iter != cacheEntry.end(); ++iter) {
      uint8_t bound = 0;

      if (cmd.offset < iter->offset + minPages) {
        bound |= 1;
      }
      if (iter->offset < cmd.offset + cmd.length) {
        bound |= 2;
      }

      if (bound != 0) {
        LPN from = MAX(cmd.offset, iter->offset);
        LPN to = from + MIN(cmd.length, minPages);

        for (LPN lpn = from; lpn < to; lpn++) {
          auto &sentry = iter->list.at(lpn - from);

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

  if (id != std::numeric_limits<uint32_t>::max()) {
    if (LIKELY(cmd.opcode == HIL::Operation::Write)) {
      // As we can start write only if PCIe DMA done, write is always partial.
      write_find(cmd.subCommandList.at(id));
    }
    else {
      panic("Unexpected opcode.");
    }
  }
  else {
    switch (cmd.opcode) {
      case HIL::Operation::Read:
        read_find(cmd);

        break;
      case HIL::Operation::Flush:
        flush_find(cmd);

        break;
      case HIL::Operation::Trim:
      case HIL::Operation::Format:
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

    usedCapacity = 0;
    dirtyCapacity = 0;
  }
  else {
    usedCapacity = totalCapacity;
    dirtyCapacity = totalCapacity;
  }
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
  BACKUP_SCALAR(out, prefetchPages);
  BACKUP_SCALAR(out, evictPages);
  BACKUP_SCALAR(out, evictPolicy);

  BACKUP_SCALAR(out, usedCapacity);
  BACKUP_SCALAR(out, enabled);
  BACKUP_SCALAR(out, prefetchEnabled);
  BACKUP_SCALAR(out, trigger.lastRequestID);
  BACKUP_SCALAR(out, trigger.requestCounter);
  BACKUP_SCALAR(out, trigger.requestCapacity);
  BACKUP_SCALAR(out, trigger.lastAddress);
  BACKUP_SCALAR(out, trigger.trigger);
  BACKUP_SCALAR(out, minPages);
  BACKUP_SCALAR(out, clock);
  BACKUP_BLOB(out, &stat, sizeof(stat));

  // backupQueue(out, &queue);

  BACKUP_EVENT(out, eventReadPreCPUDone);
  BACKUP_EVENT(out, eventReadMetaDone);
  BACKUP_EVENT(out, eventReadFTLDone);
  BACKUP_EVENT(out, eventReadDRAMDone);
  BACKUP_EVENT(out, eventReadDMADone);
  BACKUP_EVENT(out, eventWritePreCPUDone);
  BACKUP_EVENT(out, eventWriteMetaDone);
  BACKUP_EVENT(out, eventWriteDRAMDone);
  BACKUP_EVENT(out, eventEvictDRAMDone);
  BACKUP_EVENT(out, eventEvictFTLDone);
  BACKUP_EVENT(out, eventFlushPreCPUDone);
  BACKUP_EVENT(out, eventFlushMetaDone);
  BACKUP_EVENT(out, eventInvalidatePreCPUDone);
  BACKUP_EVENT(out, eventInvalidateMetaDone);
  BACKUP_EVENT(out, eventInvalidateFTLDone);
}

void RingBuffer::restoreCheckpoint(std::istream &in) noexcept {
  uint64_t tmp64;
  uint32_t tmp32;
  uint8_t tmp8;

  RESTORE_SCALAR(in, tmp64);
  panic_if(tmp64 != totalCapacity, "Line size not matched while restore.");

  RESTORE_SCALAR(in, tmp32);
  panic_if(tmp32 != prefetchPages, "Way size not matched while restore.");

  RESTORE_SCALAR(in, tmp32);
  panic_if(tmp32 != evictPages, "readEnabled not matched while restore.");

  RESTORE_SCALAR(in, tmp8);
  panic_if(tmp8 != (uint8_t)evictPolicy,
           "writeEnabled not matched while restore.");

  RESTORE_SCALAR(in, usedCapacity);
  RESTORE_SCALAR(in, enabled);
  RESTORE_SCALAR(in, prefetchEnabled);
  RESTORE_SCALAR(in, trigger.lastRequestID);
  RESTORE_SCALAR(in, trigger.requestCounter);
  RESTORE_SCALAR(in, trigger.requestCapacity);
  RESTORE_SCALAR(in, trigger.lastAddress);
  RESTORE_SCALAR(in, trigger.trigger);
  RESTORE_SCALAR(in, minPages);
  RESTORE_SCALAR(in, clock);
  RESTORE_BLOB(in, &stat, sizeof(stat));

  // restoreQueue(in, &queue);

  RESTORE_EVENT(in, eventReadPreCPUDone);
  RESTORE_EVENT(in, eventReadMetaDone);
  RESTORE_EVENT(in, eventReadFTLDone);
  RESTORE_EVENT(in, eventReadDRAMDone);
  RESTORE_EVENT(in, eventReadDMADone);
  RESTORE_EVENT(in, eventWritePreCPUDone);
  RESTORE_EVENT(in, eventWriteMetaDone);
  RESTORE_EVENT(in, eventWriteDRAMDone);
  RESTORE_EVENT(in, eventEvictDRAMDone);
  RESTORE_EVENT(in, eventEvictFTLDone);
  RESTORE_EVENT(in, eventFlushPreCPUDone);
  RESTORE_EVENT(in, eventFlushMetaDone);
  RESTORE_EVENT(in, eventInvalidatePreCPUDone);
  RESTORE_EVENT(in, eventInvalidateMetaDone);
  RESTORE_EVENT(in, eventInvalidateFTLDone);
}

}  // namespace SimpleSSD::ICL
