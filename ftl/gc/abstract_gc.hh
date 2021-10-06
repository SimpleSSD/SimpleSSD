// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_FTL_GC_ABSTRACT_GC_HH__
#define __SIMPLESSD_FTL_GC_ABSTRACT_GC_HH__

#include "fil/fil.hh"
#include "ftl/allocator/victim_selection.hh"
#include "ftl/def.hh"
#include "ftl/job_manager.hh"

namespace SimpleSSD::FTL::GC {

enum class State : uint32_t {
  /* Idle states */
  Idle,    // GC is not triggered
  Paused,  // GC has been suepended

  /* Active states */
  Foreground,  // GC triggered as foreground
  Background,  // GC triggered as background
};

class AbstractGC : public AbstractJob {
 protected:
  FIL::FIL *pFIL;
  State state;

  const Parameter *param;
  BlockAllocator::AbstractVictimSelection *method;

 public:
  AbstractGC(ObjectData &, FTLObjectData &, FIL::FIL *);
  virtual ~AbstractGC();

  bool trigger_readMapping(Request *req) final {
    requestArrived(req);

    return state >= State::Foreground;
  }

  bool trigger_readSubmit(Request *) final { return false; }

  bool trigger_readDone(Request *) final { return false; }

  bool trigger_writeMapping(Request *req) final {
    requestArrived(req);

    return state >= State::Foreground;
  }

  bool trigger_writeSubmit(Request *) final { return false; }

  bool trigger_writeDone(Request *) final {
    triggerForeground();

    return state >= State::Foreground;
  }

  /**
   * \brief GC initialization function
   *
   * Immediately call AbstractGC::initialize() when you override this function.
   */
  void initialize() override;

  /**
   * \brief Trigger foreground GC if condition met
   */
  virtual void triggerForeground() = 0;

  /**
   * \brief Notify request arrived (background GC)
   */
  virtual void requestArrived(Request *) = 0;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::FTL::GC

#endif
