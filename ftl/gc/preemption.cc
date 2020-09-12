// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "ftl/gc/preemption.hh"

#include "ftl/allocator/abstract_allocator.hh"
#include "ftl/base/abstract_ftl.hh"

namespace SimpleSSD::FTL::GC {

PreemptibleGC::PreemptibleGC(ObjectData &o, FTLObjectData &fo, FIL::FIL *f)
    : AdvancedGC(o, fo, f) {
  logid = Log::DebugID::FTL_PreemptibleGC;
}

PreemptibleGC::~PreemptibleGC() {}

void PreemptibleGC::createCheckpoint(std::ostream &out) const noexcept {
  AdvancedGC::createCheckpoint(out);
}

void PreemptibleGC::restoreCheckpoint(std::istream &in) noexcept {
  AdvancedGC::restoreCheckpoint(in);
}

}  // namespace SimpleSSD::FTL::GC
