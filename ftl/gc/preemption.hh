// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_FTL_GC_PREEMPTION_HH__
#define __SIMPLESSD_FTL_GC_PREEMPTION_HH__

#include "ftl/gc/advanced.hh"

namespace SimpleSSD::FTL::GC {

class PreemptibleGC : public AdvancedGC {
 protected:
  // Pending FIL requests (read/program/erase)
  std::vector<uint64_t> pendingFILs;

  Log::DebugID getDebugLogID() override {
    return Log::DebugID::FTL_PreemptibleGC;
  }

  void triggerBackground(uint64_t) override;

  inline bool preemptRequested() {
    return firstRequestArrival < std::numeric_limits<uint64_t>::max();
  }

  inline void increasePendingFIL(uint32_t idx) {
    pendingFILs[idx] += superpage;
  }

  inline void decreasePendingFIL(uint32_t idx) { pendingFILs[idx]--; }

  virtual inline void checkPreemptible() {
    bool allstop = true;

    for (auto &iter : pendingFILs) {
      if (iter != 0) {
        allstop = false;
        break;
      }
    }

    if (UNLIKELY(allstop)) {
      state = State::Paused;

      debugprint(logid, "GC    | Preempted");

      // Calculate penalty here
      updatePenalty(getTick());

      firstRequestArrival = std::numeric_limits<uint64_t>::max();
    }
  }

  void resumePaused();

  void readPage(uint64_t, uint32_t) override;
  void updateMapping(uint64_t, uint32_t) override;
  void writePage(uint64_t, uint32_t) override;
  void writeDone(uint64_t, uint32_t) override;
  void eraseDone(uint64_t, uint32_t) override;
  void done(uint64_t, uint32_t) override;

 public:
  PreemptibleGC(ObjectData &, FTLObjectData &, FIL::FIL *);
  virtual ~PreemptibleGC();

  void triggerForeground() override;
  void requestArrived(Request *) override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::FTL::GC

#endif
