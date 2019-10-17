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

  enum class LineStatus {
    None,
    ReadHit,
    ReadHitPending,
    ReadColdMiss,  // Cold Miss
    ReadMiss,      // Capacity/Conflict Mis
    Prefetch,      // Prefetch/Read-ahead
    WriteCache,    // Cold Miss + Hit
    WriteNVM,      // Capacity/Conflict Miss
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
  std::list<CacheContext> readPendingQueue;
  std::list<CacheContext> readEvictQueue;
  std::list<CacheContext> readMetaQueue;
  std::list<CacheContext> readFTLQueue;
  std::list<CacheContext> readDRAMQueue;
  std::list<CacheContext> readDMAQueue;

  CacheContext findRequest(std::list<CacheContext> &, uint64_t);

  // Helper
  inline uint32_t getSetIndex(LPN);
  inline bool getValidWay(uint32_t, uint32_t &);
  inline bool getEmptyWay(uint32_t, uint32_t &);
  inline Line *getLine(uint32_t, uint32_t);

  // Event
  Event eventReadPreCPUDone;
  Event eventReadMetaDone;
  Event eventReadFTLDone;
  Event eventReadDRAMDone;
  Event eventReadDMADone;

  void read_find(Request &&);
  void read_findDone(uint64_t);
  void read_doftl(uint64_t);
  void read_dodram(uint64_t);
  void read_dodma(uint64_t, uint64_t);
  void read_done(uint64_t);

  void write_find(Request &&);

  void invalidate_find(Request &&);

  void flush_find(Request &&);

  void evict(uint32_t);

  void prefetch(LPN, LPN);

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
