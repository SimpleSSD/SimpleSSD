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
#include "hil/command_manager.hh"
#include "sim/object.hh"

namespace SimpleSSD::FTL {

class AbstractFTL;

namespace BlockAllocator {

class AbstractAllocator;

}

namespace Mapping {

class AbstractMapping : public Object {
 protected:
  Parameter param;
  FIL::Config::NANDStructure *filparam;

  AbstractFTL *pFTL;
  BlockAllocator::AbstractAllocator *allocator;

 public:
  AbstractMapping(ObjectData &o) : Object(o), allocator(nullptr) {
    filparam = object.config->getNANDStructure();
    auto channel =
        readConfigUint(Section::FlashInterface, FIL::Config::Key::Channel);
    auto way = readConfigUint(Section::FlashInterface, FIL::Config::Key::Way);

    param.totalPhysicalBlocks =
        channel * way * filparam->die * filparam->plane * filparam->block;
    param.totalLogicalBlocks =
        param.totalPhysicalBlocks /
        (1.f - readConfigFloat(Section::FlashTranslation,
                               Config::Key::OverProvisioningRatio));
    param.totalPhysicalPages = param.totalPhysicalBlocks * filparam->page;
    param.totalLogicalPages = param.totalLogicalBlocks * filparam->page;
    param.pageSize = filparam->pageSize;
    param.parallelism = channel * way * filparam->die * filparam->plane;

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
    param.superpage = 1;

    for (uint8_t i = 0; i < 4; i++) {
      if (param.superpageLevel & filparam->pageAllocation[i]) {
        mask |= filparam->pageAllocation[i];
        param.superpage *= param.parallelismLevel[i];
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

  virtual void initialize(AbstractFTL *f,
                          BlockAllocator::AbstractAllocator *a) {
    pFTL = f;
    allocator = a;
  };

  virtual LPN getPageUsage(LPN, LPN) = 0;

  virtual inline LPN getLogicalBlockIndex(LPN lpn) {
    return lpn % param.totalLogicalBlocks;
  }

  virtual inline LPN getLogicalPageIndex(LPN lpn) {
    return lpn / param.totalLogicalBlocks;
  }

  virtual inline LPN getLogicalSuperBlockIndex(LPN lpn) {
    return (lpn % param.totalLogicalBlocks) / param.superpage;
  }

  virtual inline LPN getLogicalSuperPageIndex(LPN lpn) {
    return lpn % param.superpage;
  }

  virtual inline PPN getPhysicalBlockIndex(PPN ppn) {
    return ppn % param.totalPhysicalBlocks;
  }

  virtual inline PPN getPhysicalPageIndex(PPN ppn) {
    return ppn / param.totalPhysicalBlocks;
  }

  virtual inline PPN getPhysicalSuperBlockIndex(PPN ppn) {
    return (ppn % param.totalPhysicalBlocks) / param.superpage;
  }

  virtual inline PPN getPhysicalSuperPageIndex(PPN ppn) {
    return ppn % param.superpage;
  }

  virtual inline PPN makePPNIndex(PPN block, PPN page) {
    return block + page * param.totalPhysicalBlocks;
  }

  virtual inline PPN makePPNSuperIndex(PPN superblock, PPN superpage,
                                       PPN page) {
    return superblock * param.superpage + superpage +
           page * param.totalPhysicalBlocks;
  }

  // Allocator
  virtual uint32_t getValidPages(PPN) = 0;

  // I/O interfaces
  virtual void readMapping(Command &, Event) = 0;
  virtual void writeMapping(Command &, Event) = 0;
  virtual void invalidateMapping(Command &, Event) = 0;
  virtual void getBlocks(LPN, LPN, std::deque<PPN> &, Event) = 0;

  // GC interfaces
  virtual void getCopyList(CopyList &, Event) = 0;
  virtual void releaseCopyList(CopyList &) = 0;
  virtual void writeMapping(std::pair<LPN, PPN> &, Event) = 0;
};

}  // namespace Mapping

}  // namespace SimpleSSD::FTL

#endif
