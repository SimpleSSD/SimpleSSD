// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_ICL_MANAGER_BASIC_HH__
#define __SIMPLESSD_ICL_MANAGER_BASIC_HH__

#include <unordered_map>

#include "icl/manager/abstract_manager.hh"

namespace SimpleSSD::ICL::Manager {

#define debugprint_basic(req, format, ...)                                     \
  {                                                                            \
    debugprint(Log::DebugID::ICL_BasicManager,                                 \
               "%s | REQ %7" PRIu64 ":%-3u | " format,                         \
               HIL::getOperationName(req->getOpcode()), req->getParentTag(),   \
               req->getTagForLog(), ##__VA_ARGS__);                            \
  }

class BasicDetector : public SequentialDetector {
 private:
  uint64_t lastRequestTag;
  uint64_t offset;
  uint32_t length;
  uint32_t reqLength;

  uint32_t hitCounter;
  uint32_t accessCounter;

  const uint64_t triggerCount;
  const uint64_t triggerRatio;

 public:
  BasicDetector(uint32_t, uint64_t, uint64_t);

  void submitSubRequest(HIL::SubRequest *) override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &, ObjectData &) noexcept override;
};

class BasicManager : public AbstractManager {
 protected:
  SequentialDetector *detector;

  uint64_t prefetchCount;
  LPN prefetchTrigger;
  LPN lastPrefetched;

  uint64_t drainCounter;
  std::unordered_map<uint64_t, FlushContext> drainQueue;

  void drainRange(std::vector<FlushContext>::iterator,
                  std::vector<FlushContext>::iterator);

  Event eventDrainDone;
  void drainDone(uint64_t, uint64_t);

  Event eventReadDone;
  void readDone(uint64_t);

  // Statistics
  struct {
    uint64_t prefetched;  // prefetched pages
    uint64_t drained;     // evicted pages
    uint64_t hit;         // cache hit
    uint64_t miss;        // cache miss
    uint64_t eviction;    // cache eviction
  } stat;

 public:
  BasicManager(ObjectData &, ICL *, FTL::FTL *);
  ~BasicManager();

  void read(HIL::SubRequest *) override;
  void write(HIL::SubRequest *) override;
  void flush(HIL::SubRequest *) override;
  void erase(HIL::SubRequest *) override;
  void dmaDone(HIL::SubRequest *) override;

  void lookupDone(uint64_t) override;
  void cacheDone(uint64_t) override;
  void drain(std::vector<FlushContext> &) override;

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::ICL::Manager

#endif
