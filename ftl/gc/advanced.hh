// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_FTL_GC_ADVANCED_HH_
#define __SIMPLESSD_FTL_GC_ADVANCED_HH_

#include "ftl/gc/naive.hh"

namespace SimpleSSD::FTL::GC {

class AdvancedGC : public NaiveGC {
 protected:
  uint64_t idletime;

  Event eventBackgroundGC;
  void triggerBackground(uint64_t);

  void gc_trigger() override;
  void gc_start(uint64_t) override;
  // void gc_doRead(uint64_t, uint64_t) override;
  // void gc_doTranslate(uint64_t, uint64_t) override;
  // void gc_doWrite(uint64_t, uint64_t) override;
  // void gc_doErase(uint64_t, uint64_t) override;
  // void gc_done(uint64_t, uint64_t) override;

 public:
  AdvancedGC(ObjectData &, FTLObjectData &, FIL::FIL *);
  virtual ~AdvancedGC();

  void requestArrived(bool, uint32_t) override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::FTL::GC

#endif
