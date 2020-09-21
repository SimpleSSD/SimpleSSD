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
  uint64_t pendingFIL;  // Pending FIL requests (read/program/erase)

  void triggerBackground(uint64_t) override;

  inline bool preemptRequested() {
    return firstRequestArrival < std::numeric_limits<uint64_t>::max();
  }

  inline void increasePendingFIL() { pendingFIL += superpage; }
  inline void decreasePendingFIL() { pendingFIL--; }

  virtual inline void checkPreemptible() {
    if (UNLIKELY(pendingFIL == 0)) {
      state = State::Paused;

      debugprint(logid, "GC    | Preempted");

      // Calculate penalty here
      updatePenalty(getTick());

      firstRequestArrival = std::numeric_limits<uint64_t>::max();
    }
  }

  void resumePaused();

  void gc_checkDone(uint64_t) override;
  void gc_doRead(uint64_t) override;
  void gc_doTranslate(uint64_t) override;
  void gc_doWrite(uint64_t) override;
  void gc_writeDone(uint64_t) override;
  void gc_eraseDone(uint64_t) override;

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
