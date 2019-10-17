// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_ICL_SET_ASSOCIATIVE_HH__
#define __SIMPLESSD_ICL_SET_ASSOCIATIVE_HH__

#include <functional>
#include <list>
#include <random>

#include "icl/abstract_cache.hh"

namespace SimpleSSD::ICL {

class SetAssociative : public AbstractCache {
 private:
  struct Line {
    LPN tag;               // 2/4/8 bytes
    uint16_t clock;        // 2 bytes
    uint8_t dirty : 1;     // 1 bits
    uint8_t valid : 1;     // 1 bits
    uint8_t rpending : 1;  // 1 bits
    uint8_t wpending : 1;  // 1 bits
    uint8_t rsvd : 4;      // 6 bits
  };

  class PrefetchTrigger {
   private:
    friend SetAssociative;

    const uint64_t prefetchCount;  //!< # reads to trigger
    const uint64_t prefetchRatio;  //!< # pages to trigger
    uint64_t lastRequestID;
    uint64_t requestCounter;
    uint64_t requestCapacity;
    LPN lastAddress;

   public:
    PrefetchTrigger(const uint64_t, const uint64_t);

    bool trigger(Request &);
  };

  enum class LineStatus : uint8_t {
    None,
    ReadHit,
    ReadHitPending,
    ReadColdMiss,          // Cold Miss
    ReadMiss,              // Capacity/Conflict Mis
    Prefetch,              // Prefetch/Read-ahead
    WriteHitReadPending,   // Hit but line is reading
    WriteHitWritePending,  // Hit but line is writing
    WriteCache,            // Cold Miss + Hit
    WriteEvict,            // Capacity/Conflict Miss
    WriteNVM,              // No cache
    Eviction,              // In eviction
    FlushNone,             // Nothing to flush
    Flush,                 // Flush in progress
    Invalidate,            // Trim/Format in progress
  };

  struct CacheContext {
    Request req;

    uint64_t id;  // Different with req.id (Only used in cache)

    uint32_t setIdx;
    uint32_t wayIdx;

    uint64_t submittedAt;
    uint64_t finishedAt;

    LineStatus status;

    CacheContext()
        : setIdx(0),
          wayIdx(0),
          submittedAt(0),
          finishedAt(0),
          status(LineStatus::None) {}

    CacheContext(Request &r)
        : req(std::move(r)),
          setIdx(0),
          wayIdx(0),
          submittedAt(0),
          finishedAt(0),
          status(LineStatus::None) {}
  };

  using CacheQueue = std::list<CacheContext>;

  // Cache size
  uint32_t lineSize;
  uint32_t setSize;
  uint32_t waySize;

  Line *cacheMetadata;

  bool readEnabled;
  bool writeEnabled;
  bool prefetchEnabled;

  uint64_t requestCounter;

  // Prefetch
  PrefetchTrigger trigger;
  uint32_t prefetchPages;

  // Evict unit
  uint32_t evictPages;
  std::random_device rd;
  std::mt19937 mtengine;
  std::uniform_int_distribution<uint32_t> dist;
  std::function<uint32_t(uint32_t)> chooseLine;

  // For SRAM/DRAM timing
  uint64_t metaAddress;
  uint64_t metaLineSize;
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

  // Queue between states
  CacheQueue readPendingQueue;
  CacheQueue readMetaQueue;
  CacheQueue readFTLQueue;
  CacheQueue readDRAMQueue;
  CacheQueue readDMAQueue;

  CacheQueue writePendingQueue;
  CacheQueue writeMetaQueue;
  CacheQueue writeDRAMQueue;

  CacheQueue evictQueue;
  CacheQueue evictFTLQueue;

  CacheQueue flushMetaQueue;
  CacheQueue flushQueue;

  CacheQueue invalidateMetaQueue;
  CacheQueue invalidateFTLQueue;

  CacheContext findRequest(CacheQueue &, uint64_t);

  // Helper
  inline uint32_t getSetIndex(LPN);
  inline bool getValidWay(LPN, uint32_t, uint32_t &);
  inline bool getEmptyWay(uint32_t, uint32_t &);
  inline Line *getLine(uint32_t, uint32_t);

  // Read
  void read_find(Request &&);

  Event eventReadPreCPUDone;
  void read_findDone(uint64_t);

  Event eventReadMetaDone;
  void read_doftl(uint64_t);

  Event eventReadFTLDone;
  void read_dodram(uint64_t);

  Event eventReadDRAMDone;
  void read_dodma(uint64_t, uint64_t);

  Event eventReadDMADone;
  void read_done(uint64_t);

  // Write
  void write_find(Request &&);

  Event eventWritePreCPUDone;
  void write_findDone(uint64_t);

  Event eventWriteMetaDone;
  void write_dodram(uint64_t);

  Event eventWriteDRAMDone;
  void write_done(uint64_t, uint64_t);

  // Eviction
  void evict(uint32_t, bool = false);
  void markEvict(LPN, uint32_t, uint32_t);

  Event eventEvictDRAMDone;
  void evict_doftl(uint64_t);

  Event eventEvictFTLDone;
  void evict_done(uint64_t, uint64_t);

  // Flush
  void flush_find(Request &&);

  Event eventFlushPreCPUDone;
  void flush_findDone(uint64_t);

  Event eventFlushMetaDone;
  void flush_doevict(uint64_t);

  // Trim/Format
  void invalidate_find(Request &&);

  Event eventInvalidatePreCPUDone;
  void invalidate_findDone(uint64_t);

  Event eventInvalidateMetaDone;
  void invalidate_doftl(uint64_t);

  Event eventInvalidateFTLDone;
  void invalidate_done(uint64_t, uint64_t);

  // Prefetch
  void prefetch(LPN, LPN);

  void backupQueue(std::ostream &, const CacheQueue *) const;
  void restoreQueue(std::istream &, CacheQueue *);

 public:
  SetAssociative(ObjectData &, FTL::FTL *);
  ~SetAssociative();

  void enqueue(Request &&) override;

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::ICL

#endif
