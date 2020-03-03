// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "ftl/mapping/abstract_mapping.hh"

namespace SimpleSSD::FTL::Mapping {

AbstractMapping::AbstractMapping(ObjectData &o)
    : Object(o), allocator(nullptr) {
  filparam = object.config->getNANDStructure();
  auto channel =
      readConfigUint(Section::FlashInterface, FIL::Config::Key::Channel);
  auto way = readConfigUint(Section::FlashInterface, FIL::Config::Key::Way);

  param.physicalBlocks =
      channel * way * filparam->die * filparam->plane * filparam->block;
  param.logicalBlocks =
      (uint64_t)(param.physicalBlocks *
                 (1.f - readConfigFloat(Section::FlashTranslation,
                                        Config::Key::OverProvisioningRatio)));
  param.physicalPages = param.physicalBlocks * filparam->page;
  param.logicalPages = param.logicalBlocks * filparam->page;
  param.physicalPageSize = filparam->pageSize;
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

  uint8_t superpageLevel = (uint8_t)readConfigUint(
      Section::FlashTranslation, Config::Key::SuperpageAllocation);

  // Validate superpage level
  uint8_t mask = FIL::PageAllocation::None;
  uint32_t superpage = 1;

  for (uint8_t i = 0; i < 4; i++) {
    if (superpageLevel & filparam->pageAllocation[i]) {
      mask |= filparam->pageAllocation[i];
      superpage *= param.parallelismLevel[i];
    }
    else {
      break;
    }
  }

  panic_if(superpageLevel != mask, "Invalid superpage configuration detected.");

  // Print mapping Information
  debugprint(Log::DebugID::FTL, "Total physical pages: %" PRIu64,
             param.physicalPages);
  debugprint(Log::DebugID::FTL, "Physical page size: %" PRIu64,
             param.physicalPageSize);

  if (superpage != 1) {
    debugprint(Log::DebugID::FTL, "Using superpage factor %u", superpage);
  }

  debugprint(Log::DebugID::FTL, "Total logical (super) pages: %" PRIu64,
             param.logicalPages);
  debugprint(Log::DebugID::FTL, "Logical (super) page size: %u",
             param.logicalPageSize);
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

}  // namespace SimpleSSD::FTL::Mapping
