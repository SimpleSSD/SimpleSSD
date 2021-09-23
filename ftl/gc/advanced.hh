// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_FTL_GC_ADVANCED_HH__
#define __SIMPLESSD_FTL_GC_ADVANCED_HH__

#include "ftl/gc/naive.hh"

namespace SimpleSSD::FTL::GC {

class AdvancedGC : public NaiveGC {
 protected:
  uint64_t idletime;
  uint64_t lastScheduledAt;

  inline void rescheduleBackgroundGC() {
    uint64_t tick = getTick() + idletime;

    if (lastScheduledAt < tick) {
      lastScheduledAt = tick;

      if (isScheduled(eventBackgroundGC)) {
        deschedule(eventBackgroundGC);
      }

      scheduleAbs(eventBackgroundGC, 0, lastScheduledAt);
    }
  }

  Event eventBackgroundGC;
  virtual void triggerBackground(uint64_t);

  void gc_trigger() override;

 public:
  AdvancedGC(ObjectData &, FTLObjectData &, FIL::FIL *);
  virtual ~AdvancedGC();

  void requestArrived(Request *) override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::FTL::GC

#endif
