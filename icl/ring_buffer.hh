// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_ICL_RING_BUFFER_HH__
#define __SIMPLESSD_ICL_RING_BUFFER_HH__

#include <functional>
#include <random>
#include <unordered_map>

#include "icl/abstract_cache.hh"
#include "util/bitset.hh"

namespace SimpleSSD::ICL {

class RingBuffer : public AbstractCache {
 private:
  struct SubEntry {
    union {
      uint8_t data;
      struct {
        uint8_t dirty : 1;
        uint8_t rpending : 1;  // Wait for NVM Read done
        uint8_t wpending : 1;  // Wait for NVM Write done
        uint8_t rsvd : 5;
      };
    };

    Bitset valid;

    SubEntry(const SubEntry &) = delete;
    SubEntry(SubEntry &&) noexcept = default;
    SubEntry(uint32_t s) : data(0), valid(s) {}
    SubEntry(uint32_t s, bool d)
        : dirty(d), wpending(false), rsvd(0), valid(s) {}
  };

  struct Entry {
    const LPN offset;     // minPages-aligned address
    uint16_t accessedAt;  // Clock
    uint16_t insertedAt;  // Clock

    std::vector<SubEntry> list;

    Entry(const Entry &) = delete;
    Entry(Entry &&) noexcept = default;
    Entry(LPN l, uint32_t s, uint32_t b)
        : offset(l), accessedAt(0), insertedAt(0) {
      list.reserve(s);

      for (uint32_t i = 0; i < s; i++) {
        list.emplace_back(SubEntry(b));
      }
    }
  };

  using CacheEntry = std::unordered_map<LPN, Entry>;

  enum class CacheStatus {
    None,             // Invalid status
    ReadWait,         // Cache miss, we need to read data
    ReadPendingWait,  // Cache hit, we need to wait NVM read
    WriteCacheWait,   // Need eviction
  };

  enum class SelectionMode {
    All,        // From entries which has non-clean data
    FullSized,  // From entries which has full-sized (minPage) non-clean data
    Clean,      // From entries which is clean
  };

  struct CacheContext {
    CacheStatus status;

    SubCommand *scmd;
    CacheEntry::iterator entry;

    CacheContext(SubCommand *c, CacheEntry::iterator e, CacheStatus cs)
        : status(cs), scmd(c), entry(e) {}
  };

  class PrefetchTrigger {
   private:
    friend RingBuffer;  // For checkpointing

    const uint64_t prefetchCount;  //!< # reads to trigger
    const uint64_t prefetchRatio;  //!< # pages to trigger
    uint64_t requestCounter;
    uint64_t requestCapacity;
    LPN lastAddress;
    bool trigger;

   public:
    PrefetchTrigger(const uint64_t, const uint64_t);

    void update(LPN, uint32_t);
    bool triggered();
  };

  const uint32_t pageSize;  // Physical write granularity
  const uint32_t iobits;    // Necessary # bits to mark partial subentry

  // Cache size
  uint64_t maxEntryCount;
  uint64_t dirtyCapacity;

  bool enabled;
  bool prefetchEnabled;

  CacheEntry cacheEntry;

  // Prefetch
  PrefetchTrigger trigger;
  uint32_t prefetchPages;

  // Evict unit
  float triggerThreshold;
  bool noPageLimit;     // FTL allows any write size (in pageSize granularity)
  uint32_t minPages;    // FTL prefered write granularity
  uint32_t evictPages;  // Request evict pages
  std::random_device rd;
  std::mt19937 mtengine;
  std::function<CacheEntry::iterator(SelectionMode)> chooseEntry;

  // For DRAM timing
  uint64_t dataAddress;

  /**
   * Clock value for pseudo-LRU and FIFO calculation
   *
   * This value increases when request arrived.
   * When FIFO is selected, clock value of line is updated when inserted
   * When LRU is selected, clock value of line is updated when accessed
   */
  uint16_t clock;
  Config::EvictModeType evictPolicy;

  struct {
    uint64_t request[2];
    uint64_t cache[2];
  } stat;

  std::list<CacheContext> readPendingQueue;
  std::list<CacheContext> writeWaitingQueue;

  // Helper API
  inline uint64_t getDRAMAddress(LPN lpn) {
    /*
     * We have to remenber all the DRAM address <-> lpn mapping.
     * But for memory requirement and simulation speed, we approximate
     * the data address.
     */
    return dataAddress + lpn % (maxEntryCount * minPages * pageSize);
  }

  inline bool skipCheck(Bitset &bitset, uint32_t skipFront, uint32_t skipEnd) {
    uint32_t skipFrontBit = skipFront / minIO;
    uint32_t skipEndBit = skipEnd / minIO;

    panic_if(skipFront % minIO || skipEnd % minIO,
             "Skip bytes are not aligned to sector size.");

    if (bitset.none()) {
      return false;
    }
    else if (bitset.ctz() > skipFrontBit) {
      return false;
    }
    else if (bitset.clz() > skipEndBit) {
      return false;
    }

    return true;
  }

  inline void updateSkip(Bitset &bitset, uint32_t skipFront, uint32_t skipEnd) {
    uint32_t skipFrontBit = skipFront / minIO;
    uint32_t skipEndBit = skipEnd / minIO;

    panic_if(skipFront % minIO || skipEnd % minIO,
             "Skip bytes are not aligned to sector size.");

    skipEndBit = iobits - skipEndBit;

    for (; skipFrontBit < skipEndBit; skipFrontBit++) {
      bitset.set(skipFrontBit);
    }
  }

  inline void updateCapacity(bool isread, uint32_t exclude = 0) {
    if (isread) {
      stat.cache[0] += pageSize - exclude;
    }
    else {
      stat.cache[1] += pageSize - exclude;
      dirtyCapacity += pageSize;
    }
  }

  inline uint64_t alignToMinPage(LPN lpn) { return lpn / minPages * minPages; }

  inline bool isAligned(LPN lpn) { return alignToMinPage(lpn) == lpn; }

  inline bool isDirty(std::vector<SubEntry> &list) {
    bool dirty = false;

    for (auto &iter : list) {
      if (iter.dirty) {
        dirty = true;

        break;
      }
    }

    return dirty;
  }

  inline bool isFullSizeDirty(std::vector<SubEntry> &list) {
    bool good = true;

    for (auto &iter : list) {
      if (!iter.valid.all() || iter.dirty) {
        good = false;

        break;
      }
    }

    return good;
  }

  // Workers
  bool readTriggered;
  bool writeTriggered;
  uint64_t readWaitsEviction;
  LPN lastReadPendingAddress;
  LPN lastReadDoneAddress;
  std::vector<uint64_t> readWorkerTag;
  std::vector<uint64_t> writeWorkerTag;
  std::vector<uint64_t> flushEvents;

  void trigger_readWorker();

  Event eventReadWorker;
  void readWorker();

  Event eventReadWorkerDoFTL;
  void readWorker_doFTL();

  Event eventReadWorkerDone;
  void readWorker_done(uint64_t);

  void trigger_writeWorker();

  Event eventWriteWorker;
  void writeWorker();

  Event eventWriteWorkerDoFTL;
  void writeWorker_doFTL();

  Event eventWriteWorkerDone;
  void writeWorker_done(uint64_t);

  // Read
  void read_find(Command &);

  Event eventReadPreCPUDone;
  void read_findDone(uint64_t);

  Event eventReadDRAMDone;
  void read_done(uint64_t);

  // Write
  void write_find(SubCommand &);

  Event eventWritePreCPUDone;
  void write_findDone(uint64_t);

  Event eventWriteDRAMDone;
  void write_done(uint64_t);

  // Flush
  void flush_find(Command &);

  // Trim/Format
  void invalidate_find(Command &);

 public:
  RingBuffer(ObjectData &, CommandManager *, FTL::FTL *);

  void enqueue(uint64_t, uint32_t) override;

  void setCache(bool) override;
  bool getCache() override;

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::ICL

#endif
