// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_FTL_BACKGROUND_MANAGER_BASIC_JOB_MANAGER_HH__
#define __SIMPLESSD_FTL_BACKGROUND_MANAGER_BASIC_JOB_MANAGER_HH__

#include "ftl/background_manager/abstract_job_manager.hh"

namespace SimpleSSD::FTL {

class BasicJobManager : public AbstractJobManager {
 protected:
  std::vector<AbstractJob *> jobs;

  // Threshold based idletime detection
  const uint64_t threshold;
  uint64_t lastScheduledAt;

  inline void rescheduleIdletimeDetection(uint64_t now) {
    uint64_t tick = now + threshold;

    if (lastScheduledAt < tick) {
      lastScheduledAt = tick;

      if (isScheduled(eventIdletime)) {
        deschedule(eventIdletime);
      }

      scheduleAbs(eventIdletime, 0, lastScheduledAt);
    }
  }

  inline void descheduleIdletimeDetection() {
    lastScheduledAt = 0;

    if (isScheduled(eventIdletime)) {
      deschedule(eventIdletime);
    }
  }

  // Statistics
  struct {
    uint64_t count;   // # background job invocation in idletime
    uint64_t usable;  // Total usable idletime by background jobs
    uint64_t wasted;  // Total wasted idletime by threshold
  } stat;

  uint64_t lastCompleteAt;

  inline void markUserComplete(uint64_t now) { lastCompleteAt = now; }
  inline void markUserMapping(uint64_t now) {
    if (lastCompleteAt > 0) {
      if (now <= lastScheduledAt) {
        // wasted
        stat.wasted += now - lastCompleteAt;
      }
      else {
        // usable
        stat.usable += now - lastScheduledAt;
      }

      lastCompleteAt = 0;
    }
  }

  Event eventIdletime;
  virtual void idletimeEvent(uint64_t);

 public:
  BasicJobManager(ObjectData &);
  ~BasicJobManager();

  void addBackgroundJob(AbstractJob *) override;

  void initialize(bool) override;
  bool isRunning() override;

  void triggerByUser(TriggerType, Request *) override;

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::FTL

#endif
