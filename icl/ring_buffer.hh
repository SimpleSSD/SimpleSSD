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

#include "icl/abstract_cache.hh"
#include "util/bitset.hh"

namespace SimpleSSD::ICL {

class RingBuffer : public AbstractCache {
 private:
  struct SubEntry {
    uint8_t dirty : 1;
    uint8_t wpending : 1;  // Wait for NVM Write done
    uint8_t rsvd : 6;

    SubEntry() : dirty(false), wpending(false) {}
    SubEntry(bool d) : dirty(d), wpending(false) {}
  };

  struct Entry {
    LPN offset;
    uint32_t length;
    uint16_t accessedAt;  // Clock

    Bitset skipFront;  // 0 if no data exists (from LSB)
    Bitset skipEnd;    // 0 if no data exists (from MSB)

    std::deque<SubEntry> list;

    Entry(uint32_t s)
        : offset(0), length(0), accessedAt(0), skipFront(s), skipEnd(s) {}
  };

  enum class CacheStatus {
    None,            // Invalid status
    ReadWait,        // Cache miss, we need to read data
    WriteWait,       // We need to write data (cache disabled)
    WriteCacheWait,  // Need eviction

    FTL,  // Submitted to FTL, waiting for completion
  };

  enum class SelectionMode {
    All,        // From entries which has non-clean data
    FullSized,  // From entries which has full-sized (minPage) non-clean data
    Clean,      // From entries which is clean
  };

  struct CacheContext {
    CacheStatus status;

    HIL::SubCommand *scmd;
    std::list<Entry>::iterator entry;

    CacheContext(HIL::SubCommand *c, std::list<Entry>::iterator e,
                 CacheStatus cs)
        : status(cs), scmd(c), entry(e) {}
  };

  class PrefetchTrigger {
   private:
    friend RingBuffer;  // For checkpointing

    const uint64_t prefetchCount;  //!< # reads to trigger
    const uint64_t prefetchRatio;  //!< # pages to trigger
    uint64_t lastRequestID;
    uint64_t requestCounter;
    uint64_t requestCapacity;
    LPN lastAddress;
    bool trigger;

   public:
    PrefetchTrigger(const uint64_t, const uint64_t);

    void update(uint64_t, LPN, uint32_t);
    bool triggered();
  };

  const uint32_t pageSize;  // Physical write granularity
  const uint32_t iobits;    // Necessary # bits to mark partial subentry

  // Cache size
  uint64_t totalCapacity;
  uint64_t usedCapacity;
  uint64_t dirtyCapacity;

  bool enabled;
  bool prefetchEnabled;

  std::list<Entry> cacheEntry;

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
  std::function<std::list<Entry>::iterator(SelectionMode)> chooseEntry;

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
  std::list<CacheContext> writePendingQueue;
  std::list<CacheContext> writeWaitingQueue;

  // Helper API
  inline uint64_t getDRAMAddress(LPN lpn) {
    /*
     * We have to remenber all the DRAM address <-> lpn mapping.
     * But for memory requirement and simulation speed, we approximate
     * the data address.
     */
    return dataAddress + lpn % totalCapacity;
  }

  inline bool skipCheck(Bitset &bitset, uint32_t skip, bool front) {
    uint32_t skipBit = skip / minIO;

    panic_if(skip % minIO, "Skip bytes are not aligned to sector size.");

    if (front && bitset.ffs() < skipBit) {
      return false;
    }
    else if (!front && bitset.clz() < skipBit) {
      return false;
    }

    return true;
  }

  inline void updateSkip(Bitset &bitset, uint32_t skip, bool front) {
    uint32_t skipBit = skip / minIO;

    panic_if(skip % minIO, "Skip bytes are not aligned to sector size.");

    if (front) {
      // Add bits
      for (; skipBit < iobits; skipBit++) {
        bitset.set(skipBit);
      }
    }
    else {
      // Add bits
      skipBit = iobits - skipBit;

      for (uint32_t i = 0; i < skipBit; i++) {
        bitset.set(i);
      }
    }
  }

  inline void updateCapacity(bool isread, uint32_t exclude = 0) {
    if (isread) {
      stat.cache[0] += pageSize - exclude;
      usedCapacity += pageSize;
    }
    else {
      stat.cache[1] += pageSize - exclude;
      usedCapacity += pageSize;
      dirtyCapacity += pageSize;
    }
  }

  inline uint64_t alignToMinPage(LPN lpn) { return lpn / minPages * minPages; }

  inline bool isAligned(LPN lpn) { return alignToMinPage(lpn) == lpn; }

  inline bool isDirty(std::deque<SubEntry> &list) {
    bool dirty = false;

    for (auto &iter : list) {
      if (iter.dirty) {
        dirty = true;

        break;
      }
    }

    return dirty;
  }

  // Workers
  bool readTriggered;
  bool writeTriggered;
  std::vector<uint64_t> readWorkerTag;

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
  void read_find(HIL::Command &);

  Event eventReadPreCPUDone;
  void read_findDone(uint64_t);

  Event eventReadDRAMDone;
  void read_done(uint64_t);

  // Write
  void write_find(HIL::SubCommand &);

  Event eventWritePreCPUDone;
  void write_findDone(uint64_t);

  Event eventWriteDRAMDone;
  void write_done(uint64_t);

 public:
  RingBuffer(ObjectData &, HIL::CommandManager *, FTL::FTL *);

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
