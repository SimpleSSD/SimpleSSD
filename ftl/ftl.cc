// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "ftl/ftl.hh"

namespace SimpleSSD {

namespace FTL {

FTL::FTL(ObjectData &o) : Object(o) {
  pFIL = new FIL::FIL(object);

  switch ((Config::MappingType)readConfigUint(Section::FlashTranslation,
                                              Config::Key::MappingMode)) {
    case Config::MappingType::PageLevelFTL:
      // pMapper = new Mapping::PageLevel(object);

      break;
    case Config::MappingType::BlockLevelFTL:
      // pMapper = new Mapping::BlockLevel(object);

      break;
    case Config::MappingType::VLFTL:
      // pMapper = new Mapping::VLFTL(object);

      break;
  }

  // Currently, we only have default block allocator
  // pAllocator = new BlockAllocator::Default(object, pMapper);

  // Initialize pFTL
  pMapper->initialize(pAllocator);
}

FTL::~FTL() {
  delete pFIL;
  delete pMapper;
  delete pAllocator;
}

Parameter *FTL::getInfo() {
  return pMapper->getInfo();
}

LPN FTL::getPageUsage(LPN lpnBegin, LPN lpnEnd) {
  return pMapper->getPageUsage(lpnBegin, lpnEnd);
}

void FTL::getStatList(std::vector<Stat> &list, std::string prefix) noexcept {
  pMapper->getStatList(list, prefix + "ftl.mapper.");
  pAllocator->getStatList(list, prefix + "ftl.allocator.");
  pFIL->getStatList(list, prefix);
}

void FTL::getStatValues(std::vector<double> &values) noexcept {
  pMapper->getStatValues(values);
  pAllocator->getStatValues(values);
  pFIL->getStatValues(values);
}

void FTL::resetStatValues() noexcept {
  pMapper->resetStatValues();
  pAllocator->resetStatValues();
  pFIL->resetStatValues();
}

void FTL::createCheckpoint(std::ostream &out) const noexcept {
  pMapper->createCheckpoint(out);
  pAllocator->createCheckpoint(out);
  pFIL->createCheckpoint(out);
}

void FTL::restoreCheckpoint(std::istream &in) noexcept {
  pMapper->restoreCheckpoint(in);
  pAllocator->restoreCheckpoint(in);
  pFIL->restoreCheckpoint(in);
}

}  // namespace FTL

}  // namespace SimpleSSD
