// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "ftl/mapping/abstract_mapping.hh"

namespace SimpleSSD::FTL::Mapping {

AbstractMapping::AbstractMapping(ObjectData &o, CommandManager *c)
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

void AbstractMapping::makeSpare(LPN lpn, std::vector<uint8_t> &spare) {
  if (spare.size() != sizeof(LPN)) {
    spare.resize(sizeof(LPN));
  }

  memcpy(spare.data(), &lpn, sizeof(LPN));
}

LPN AbstractMapping::readSpare(std::vector<uint8_t> &spare) {
  LPN lpn = InvalidLPN;

  panic_if(spare.size() < sizeof(LPN), "Empty spare data.");

  memcpy(&lpn, spare.data(), sizeof(LPN));

  return lpn;
}

void AbstractMapping::initialize(AbstractFTL *f,
                                 BlockAllocator::AbstractAllocator *a) {
  pFTL = f;
  allocator = a;
};

Parameter *AbstractMapping::getInfo() {
  return &param;
};

//! PPN -> SPIndex (Page index in superpage)
inline PPN AbstractMapping::getSPIndexFromPPN(PPN ppn) {
  return ppn % param.superpage;
}

//! LPN -> SLPN / PPN -> SPPN
inline LPN AbstractMapping::getSLPNfromLPN(LPN slpn) {
  return slpn / param.superpage;
}

//! SPPN -> SBLK
inline PPN AbstractMapping::getSBFromSPPN(PPN sppn) {
  return sppn % (param.totalPhysicalBlocks / param.superpage);
}

//! PPN -> BLK
inline PPN AbstractMapping::getBlockFromPPN(PPN ppn) {
  return ppn % param.totalPhysicalBlocks;
}

//! SBLK/SPIndex -> BLK
inline PPN AbstractMapping::getBlockFromSB(PPN sblk, PPN sp) {
  return sblk * param.superpage + sp;
}

//! SPPN -> Page (Page index in (super)block)
inline PPN AbstractMapping::getPageIndexFromSPPN(PPN sppn) {
  return sppn / (param.totalPhysicalBlocks / param.superpage);
}

//! SBLK/Page -> SPPN
inline PPN AbstractMapping::makeSPPN(PPN superblock, PPN page) {
  return superblock + page * (param.totalPhysicalBlocks / param.superpage);
}

//! SBLK/SPIndex/Page -> PPN
inline PPN AbstractMapping::makePPN(PPN superblock, PPN superpage, PPN page) {
  return superblock * param.superpage + superpage +
         page * param.totalPhysicalBlocks;
}

//! Mapping granularity
inline LPN AbstractMapping::mappingGranularity() {
  return param.superpage;
}

inline void AbstractMapping::readMapping(Command &cmd, Event eid) {
  CPU::Function fstat = readMapping(cmd);

  scheduleFunction(CPU::CPUGroup::FlashTranslationLayer, eid, cmd.tag, fstat);
}

inline void AbstractMapping::writeMapping(Command &cmd, Event eid) {
  CPU::Function fstat = writeMapping(cmd);

  scheduleFunction(CPU::CPUGroup::FlashTranslationLayer, eid, cmd.tag, fstat);
}

inline void AbstractMapping::invalidateMapping(Command &cmd, Event eid) {
  CPU::Function fstat = invalidateMapping(cmd);

  scheduleFunction(CPU::CPUGroup::FlashTranslationLayer, eid, cmd.tag, fstat);
}

}  // namespace SimpleSSD::FTL::Mapping
