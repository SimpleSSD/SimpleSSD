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

struct BlockMetadata {
  PPN blockID;

  uint32_t nextPageToWrite;
  uint16_t clock;  // For cost benefit
  Bitset validPages;

  BlockMetadata() : blockID(InvalidPPN), nextPageToWrite(0), clock(0) {}
  BlockMetadata(PPN id, uint32_t s)
      : blockID(id), nextPageToWrite(0), clock(0), validPages(s) {}
};

class AbstractMapping : public Object {
 protected:
  CommandManager *commandManager;

  Parameter param;
  FIL::Config::NANDStructure *filparam;

  AbstractFTL *pFTL;
  BlockAllocator::AbstractAllocator *allocator;

  virtual void makeSpare(LPN lpn, std::vector<uint8_t> &spare) {
    if (spare.size() != sizeof(LPN)) {
      spare.resize(sizeof(LPN));
    }

    memcpy(spare.data(), &lpn, sizeof(LPN));
  }

  virtual LPN readSpare(std::vector<uint8_t> &spare) {
    LPN lpn = InvalidLPN;

    panic_if(spare.size() < sizeof(LPN), "Empty spare data.");

    memcpy(&lpn, spare.data(), sizeof(LPN));

    return lpn;
  }

 public:
  AbstractMapping(ObjectData &o, CommandManager *c)
      : Object(o), commandManager(c), allocator(nullptr) {
    filparam = object.config->getNANDStructure();
    auto channel =
        readConfigUint(Section::FlashInterface, FIL::Config::Key::Channel);
    auto way = readConfigUint(Section::FlashInterface, FIL::Config::Key::Way);

    param.totalPhysicalBlocks =
        channel * way * filparam->die * filparam->plane * filparam->block;
    param.totalLogicalBlocks =
        (uint64_t)(param.totalPhysicalBlocks *
                   (1.f - readConfigFloat(Section::FlashTranslation,
                                          Config::Key::OverProvisioningRatio)));
    param.totalPhysicalPages = param.totalPhysicalBlocks * filparam->page;
    param.totalLogicalPages = param.totalLogicalBlocks * filparam->page;
    param.pageSize = filparam->pageSize;
    param.parallelism = channel * way * filparam->die * filparam->plane;

    for (uint8_t i = 0; i < 4; i++) {
      switch (filparam->pageAllocation[i]) {
        case FIL::PageAllocation::Channel:
          param.parallelismLevel[i] = (uint32_t)channel;
          break;
        case FIL::PageAllocation::Way:
          param.parallelismLevel[i] = (uint32_t)way;
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

    param.superpageLevel = (uint8_t)readConfigUint(
        Section::FlashTranslation, Config::Key::SuperpageAllocation);

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

    param.superpageLevel = (uint8_t)popcount8(mask);

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

  //! PPN -> SPIndex (Page index in superpage)
  virtual inline PPN getSPIndexFromPPN(PPN ppn) {
    return ppn % param.superpage;
  }

  //! LPN -> SLPN / PPN -> SPPN
  virtual inline LPN getSLPNfromLPN(LPN slpn) { return slpn / param.superpage; }

  //! SPPN -> SBLK
  virtual inline PPN getSBFromSPPN(PPN sppn) {
    return sppn % (param.totalPhysicalBlocks / param.superpage);
  }

  //! PPN -> BLK
  virtual inline PPN getBlockFromPPN(PPN ppn) {
    return ppn % param.totalPhysicalBlocks;
  }

  //! SBLK/SPIndex -> BLK
  virtual inline PPN getBlockFromSB(PPN sblk, PPN sp) {
    return sblk * param.superpage + sp;
  }

  //! SPPN -> Page (Page index in (super)block)
  virtual inline PPN getPageIndexFromSPPN(PPN sppn) {
    return sppn / (param.totalPhysicalBlocks / param.superpage);
  }

  //! SBLK/Page -> SPPN
  virtual inline PPN makeSPPN(PPN superblock, PPN page) {
    return superblock + page * (param.totalPhysicalBlocks / param.superpage);
  }

  //! SBLK/SPIndex/Page -> PPN
  virtual inline PPN makePPN(PPN superblock, PPN superpage, PPN page) {
    return superblock * param.superpage + superpage +
           page * param.totalPhysicalBlocks;
  }

  //! Mapping granularity
  virtual inline LPN mappingGranularity() { return param.superpage; }

  // Allocator
  virtual uint32_t getValidPages(PPN) = 0;
  virtual uint16_t getAge(PPN) = 0;

  // I/O interfaces
  virtual CPU::Function readMapping(Command &) = 0;
  virtual CPU::Function writeMapping(Command &) = 0;
  virtual CPU::Function invalidateMapping(Command &) = 0;

  inline void readMapping(Command &cmd, Event eid) {
    CPU::Function fstat = readMapping(cmd);

    scheduleFunction(CPU::CPUGroup::FlashTranslationLayer, eid, cmd.tag, fstat);
  }

  inline void writeMapping(Command &cmd, Event eid) {
    CPU::Function fstat = writeMapping(cmd);

    scheduleFunction(CPU::CPUGroup::FlashTranslationLayer, eid, cmd.tag, fstat);
  }

  inline void invalidateMapping(Command &cmd, Event eid) {
    CPU::Function fstat = invalidateMapping(cmd);

    scheduleFunction(CPU::CPUGroup::FlashTranslationLayer, eid, cmd.tag, fstat);
  }

  // GC interfaces
  virtual void getCopyList(CopyList &, Event) = 0;
  virtual void releaseCopyList(CopyList &) = 0;
};

}  // namespace Mapping

}  // namespace SimpleSSD::FTL

#endif
