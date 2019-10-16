// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "ftl/ftl.hh"

#include "ftl/abstract_ftl.hh"

namespace SimpleSSD {

namespace FTL {

FTL::FTL(ObjectData &o) : Object(o) {
  pFIL = new FIL::FIL(object);

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

  // Yout MUST NOT REORDER Parameter structure
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
    }
  }

  if (readConfigBoolean(Section::FlashTranslation, Config::Key::UseSuperpage)) {
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

  switch ((Config::MappingType)readConfigUint(Section::FlashTranslation,
                                              Config::Key::MappingMode)) {
    case Config::MappingType::PageLevelFTL:
      break;
    case Config::MappingType::BlockLevelFTL:
      break;
    case Config::MappingType::VLFTL:
      break;
  }

  // Print mapping Information
  debugprint(Log::DebugID::FTL, "Total physical pages %u",
             param.totalPhysicalPages);
  debugprint(Log::DebugID::FTL, "Total logical pages %u",
             param.totalLogicalPages);
  debugprint(Log::DebugID::FTL, "Logical page size %u", param.pageSize);

  // Initialize pFTL
  pFTL->initialize();
}

FTL::~FTL() {
  delete pFIL;
  delete pFTL;
}

Parameter *FTL::getInfo() {
  return &param;
}

LPN FTL::getPageUsage(LPN lpnBegin, LPN lpnEnd) {
  return pFTL->getStatus(lpnBegin, lpnEnd)->mappedLogicalPages;
}

void FTL::getStatList(std::vector<Stat> &list, std::string prefix) noexcept {
  pFTL->getStatList(list, prefix + "ftl.");
  pFIL->getStatList(list, prefix);
}

void FTL::getStatValues(std::vector<double> &values) noexcept {
  pFTL->getStatValues(values);
  pFIL->getStatValues(values);
}

void FTL::resetStatValues() noexcept {
  pFTL->resetStatValues();
  pFIL->resetStatValues();
}

void FTL::createCheckpoint(std::ostream &out) const noexcept {
  BACKUP_BLOB(out, &param, sizeof(Parameter));

  pFTL->createCheckpoint(out);
  pFIL->createCheckpoint(out);
}

void FTL::restoreCheckpoint(std::istream &in) noexcept {
  RESTORE_BLOB(in, &param, sizeof(Parameter));

  pFTL->restoreCheckpoint(in);
  pFIL->restoreCheckpoint(in);
}

}  // namespace FTL

}  // namespace SimpleSSD
