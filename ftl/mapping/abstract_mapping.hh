// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_FTL_MAPPING_ABSTRACT_MAPPING_HH__
#define __SIMPLESSD_FTL_MAPPING_ABSTRACT_MAPPING_HH__

#include "ftl/def.hh"
#include "sim/object.hh"

namespace SimpleSSD::FTL {

namespace BlockAllocator {

class AbstractAllocator;

}

namespace Mapping {

class AbstractMapping : public Object {
 protected:
  Parameter param;
  BlockAllocator::AbstractAllocator *allocator;

 public:
  AbstractMapping(ObjectData &o) : Object(o), allocator(nullptr) {
    auto filparam = object.config->getNANDStructure();
    auto channel =
        readConfigUint(Section::FlashInterface, FIL::Config::Key::Channel);
    auto way = readConfigUint(Section::FlashInterface, FIL::Config::Key::Way);

    param.totalPhysicalPages = channel * way * filparam->die * filparam->plane *
                               filparam->block * filparam->plane;
    param.totalLogicalPages =
        param.totalPhysicalPages /
        (1.f - readConfigFloat(Section::FlashTranslation,
                               Config::Key::OverProvisioningRatio));
    param.pageSize = filparam->pageSize;

    for (uint8_t i = 0; i < 4; i++) {
      switch (filparam->pageAllocation[i]) {
        case FIL::PageAllocation::Channel:
          param.parallelismLevel[i] = channel;
          break;
        case FIL::PageAllocation::Way:
          param.parallelismLevel[i] = way;
          break;
        case FIL::PageAllocation::Die:
          param.parallelismLevel[i] = filparam->die;
          break;
        case FIL::PageAllocation::Plane:
          param.parallelismLevel[i] = filparam->plane;
          break;
        default:
          break;
      }
    }

    if (readConfigBoolean(Section::FlashTranslation,
                          Config::Key::UseSuperpage)) {
      param.superpageLevel = readConfigUint(Section::FlashTranslation,
                                            Config::Key::SuperpageAllocation);
    }
    else {
      param.superpageLevel = FIL::PageAllocation::None;
    }

    // Validate superpage level
    uint8_t mask = FIL::PageAllocation::None;

    for (uint8_t i = 0; i < 4; i++) {
      if (param.superpageLevel | filparam->pageAllocation[i]) {
        mask |= filparam->pageAllocation[i];
      }
      else {
        break;
      }
    }

    panic_if(param.superpageLevel != mask,
             "Invalid superpage configuration detected.");

    param.superpageLevel = popcount16(mask);

    // Print mapping Information
    debugprint(Log::DebugID::FTL, "Total physical pages %u",
               param.totalPhysicalPages);
    debugprint(Log::DebugID::FTL, "Total logical pages %u",
               param.totalLogicalPages);
    debugprint(Log::DebugID::FTL, "Logical page size %u", param.pageSize);
  }

  AbstractMapping(const AbstractMapping &) = delete;
  AbstractMapping(AbstractMapping &&) noexcept = default;
  virtual ~AbstractMapping() {}

  AbstractMapping &operator=(const AbstractMapping &) = delete;
  AbstractMapping &operator=(AbstractMapping &&) = default;

  Parameter *getInfo() { return &param; };

  virtual void initialize(BlockAllocator::AbstractAllocator *) = 0;

  virtual LPN getPageUsage(LPN, LPN) = 0;

  virtual FTLContext &getContext(uint64_t) = 0;
  virtual void releaseContext(uint64_t) = 0;

  // I/O interfaces
  virtual void readMapping(Request &&, Event) = 0;
  virtual void writeMapping(Request &&, Event) = 0;
  virtual void invalidateMapping(Request &&, Event) = 0;
  virtual void getBlocks(LPN, LPN, std::deque<PPN> &, Event) = 0;

  // GC interfaces
  virtual bool checkGCThreshold() = 0;
  virtual void getBlockInfo(BlockInfo &, Event) = 0;
  virtual void writeMapping(std::pair<LPN, PPN> &, Event) = 0;
};

}  // namespace Mapping

}  // namespace SimpleSSD::FTL

#endif
