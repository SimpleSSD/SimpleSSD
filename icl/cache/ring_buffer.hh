// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_ICL_CACHE_RING_BUFFER_HH__
#define __SIMPLESSD_ICL_CACHE_RING_BUFFER_HH__

#include "icl/cache/abstract_cache.hh"

namespace SimpleSSD::ICL {

class RingBuffer : public AbstractCache {
 protected:
  uint32_t cacheTagSize;
  uint32_t cacheDataSize;
  uint64_t cacheTagBaseAddress;
  uint64_t cacheDataBaseAddress;

  uint64_t evictThreshold;
  uint64_t dirtyLines;

  std::vector<CacheLine> cacheline;
  // Evict function
  // compare function

  // Lookup pending

  // Miss pending

  // Allocate pending

  // Flush pending

  // Evict pending

  // Victim selection functions

  // Helper functions

 public:
  RingBuffer(ObjectData &, AbstractManager *, FTL::Parameter *);
  ~RingBuffer();

  void lookup(HIL::SubRequest *) override;
  void flush(HIL::SubRequest *) override;
  void erase(HIL::SubRequest *) override;
  void allocate(HIL::SubRequest *) override;
  void dmaDone(LPN) override;
  void nvmDone(LPN, bool) override;

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::ICL

#endif
