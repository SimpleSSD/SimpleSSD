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

  Event eventBackgroundGC;
  virtual void triggerBackground(uint64_t);

  void gc_trigger() override;
  void gc_checkDone(uint64_t) override;

 public:
  AdvancedGC(ObjectData &, FTLObjectData &, FIL::FIL *);
  virtual ~AdvancedGC();

  void requestArrived(Request *) override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::FTL::GC

#endif
