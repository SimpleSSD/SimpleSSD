// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "ftl/gc/preemption.hh"

namespace SimpleSSD::FTL::GC {

PreemptibleGC::PreemptibleGC(ObjectData &o, FTLObjectData &fo, FIL::FIL *f)
    : AdvancedGC(o, fo, f) {
  logid = Log::DebugID::FTL_PreemptibleGC;
}

PreemptibleGC::~PreemptibleGC() {}

void PreemptibleGC::gc_doRead(uint64_t now, uint64_t tag) {
  if (LIKELY(!preemptRequested())) {
    // Don't use AdvancedGC::gc_doRead
    NaiveGC::gc_doRead(now, tag);

    increasePendingFIL();
  }
  else {
    checkPreemptible();
  }
}

}  // namespace SimpleSSD::FTL::GC
