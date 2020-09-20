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
  void gc_doRead(uint64_t, uint64_t) override;

 public:
  PreemptibleGC(ObjectData &, FTLObjectData &, FIL::FIL *);
  virtual ~PreemptibleGC();
};

}  // namespace SimpleSSD::FTL::GC

#endif
