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
#include "ftl/background_manager/abstract_background_job.hh"
#include "ftl/def.hh"

namespace SimpleSSD::FTL::GC {

enum class State : uint32_t {
  /* Idle states */
  Idle,    // GC is not triggered
  Paused,  // GC has been suepended

  /* Active states */
  Foreground,  // GC triggered as foreground
  Background,  // GC triggered as background
};

class AbstractGC : public AbstractBlockCopyJob {
 protected:
  State state;

  BlockAllocator::AbstractVictimSelection *method;

 public:
  AbstractGC(ObjectData &, FTLObjectData &, FIL::FIL *);
  virtual ~AbstractGC();

  /**
   * \brief GC initialization function
   *
   * Immediately call AbstractGC::initialize() when you override this function.
   */
  void initialize() override;

  /**
   * \brief Return GC is running
   *
   * \return true if GC is in progress
   */
  bool isRunning() override;

  void triggerByUser(TriggerType, Request *) override;

  /* GC-specific APIs */

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
