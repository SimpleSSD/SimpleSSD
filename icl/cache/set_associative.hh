// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_ICL_CACHE_SET_ASSOCIATIVE_HH__
#define __SIMPLESSD_ICL_CACHE_SET_ASSOCIATIVE_HH__

#include <random>

#include "icl/cache/abstract_cache.hh"
#include "util/bitset.hh"

namespace SimpleSSD::ICL {

class SetAssociative : public AbstractCache {
 protected:
  struct CacheLine {
    union {
      uint8_t data;
      struct {
        uint8_t valid : 1;
        uint8_t dirty : 1;
        uint8_t nvmPending : 1;
        uint8_t dmaPending : 1;
        uint8_t rsvd : 4;
      };
    };

    LPN tag;
    uint64_t insertedAt;
    uint64_t accessedAt;
    Bitset validbits;

    CacheLine(uint64_t size)
        : data(0), tag(0), insertedAt(0), accessedAt(0), validbits(size) {}
  };

  uint32_t sectorsInCacheLine;
  uint32_t setSize;
  uint32_t waySize;

  Config::Granularity evictMode;
  uint32_t pagesToEvict;

  uint32_t cacheTagSize;
  uint64_t cacheTagBaseAddress;
  uint64_t cacheDataBaseAddress;

  std::vector<CacheLine> cacheline;
  std::function<CPU::Function(uint32_t, uint32_t &)> evictFunction;

  // Pending queues

  // Victim selection
  std::random_device rd;
  std::mt19937 gen;
  std::uniform_int_distribution<uint32_t> dist;

  CPU::Function randomEviction(uint32_t, uint32_t &);
  CPU::Function fifoEviction(uint32_t, uint32_t &);
  CPU::Function lruEviction(uint32_t, uint32_t &);

  inline uint32_t getSetIdx(LPN addr) { return addr % setSize; }
  CPU::Function getEmptyWay(uint32_t, uint32_t &);
  CPU::Function getValidWay(LPN, uint32_t &);

 public:
  SetAssociative(ObjectData &, AbstractManager *, FTL::Parameter *);
  ~SetAssociative();

  CPU::Function lookup(SubRequest *, bool) override;
  CPU::Function allocate(SubRequest *) override;
  CPU::Function flush(SubRequest *) override;
  CPU::Function erase(SubRequest *) override;
  void dmaDone(LPN) override;
  void drainDone() override;

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::ICL

#endif
