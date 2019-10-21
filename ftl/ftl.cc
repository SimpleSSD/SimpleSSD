// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "ftl/ftl.hh"

#include "ftl/allocator/basic_allocator.hh"
#include "ftl/base/basic_ftl.hh"
#include "ftl/mapping/page_level.hh"

namespace SimpleSSD {

namespace FTL {

FTL::FTL(ObjectData &o, CommandManager *m) : Object(o), commandManager(m) {
  pFIL = new FIL::FIL(object, commandManager);

  switch ((Config::MappingType)readConfigUint(Section::FlashTranslation,
                                              Config::Key::MappingMode)) {
    case Config::MappingType::PageLevelFTL:
      pMapper = new Mapping::PageLevel(object, commandManager);

      break;
    // case Config::MappingType::BlockLevelFTL:
    //   pMapper = new Mapping::BlockLevel(object);

    //   break;
    // case Config::MappingType::VLFTL:
    //   pMapper = new Mapping::VLFTL(object);

    //   break;
    default:
      panic("Unsupported mapping type.");

      break;
  }

  // Currently, we only have default block allocator
  pAllocator = new BlockAllocator::BasicAllocator(object, pMapper);

  // Currently, we only have default FTL
  pFTL = new BasicFTL(object, commandManager, pFIL, pMapper, pAllocator);

  // Initialize all
  pAllocator->initialize(pMapper->getInfo());
  pMapper->initialize(pFTL, pAllocator);

  pFTL->initialize();
}

FTL::~FTL() {
  delete pFIL;
  delete pMapper;
  delete pAllocator;
}

void FTL::submit(uint64_t tag) {
  pFTL->submit(tag);
}

Parameter *FTL::getInfo() {
  return pMapper->getInfo();
}

LPN FTL::getPageUsage(LPN lpnBegin, LPN lpnEnd) {
  return pMapper->getPageUsage(lpnBegin, lpnEnd);
}

bool FTL::isGC() {
  return pFTL->isGC();
}

uint8_t FTL::isFormat() {
  return pFTL->isFormat();
}

void FTL::getStatList(std::vector<Stat> &list, std::string prefix) noexcept {
  pFTL->getStatList(list, prefix + "ftl.");
  pMapper->getStatList(list, prefix + "ftl.mapper.");
  pAllocator->getStatList(list, prefix + "ftl.allocator.");
  pFIL->getStatList(list, prefix);
}

void FTL::getStatValues(std::vector<double> &values) noexcept {
  pFTL->getStatValues(values);
  pMapper->getStatValues(values);
  pAllocator->getStatValues(values);
  pFIL->getStatValues(values);
}

void FTL::resetStatValues() noexcept {
  pFTL->resetStatValues();
  pMapper->resetStatValues();
  pAllocator->resetStatValues();
  pFIL->resetStatValues();
}

void FTL::createCheckpoint(std::ostream &out) const noexcept {
  pFTL->createCheckpoint(out);
  pMapper->createCheckpoint(out);
  pAllocator->createCheckpoint(out);
  pFIL->createCheckpoint(out);
}

void FTL::restoreCheckpoint(std::istream &in) noexcept {
  pFTL->restoreCheckpoint(in);
  pMapper->restoreCheckpoint(in);
  pAllocator->restoreCheckpoint(in);
  pFIL->restoreCheckpoint(in);
}

}  // namespace FTL

}  // namespace SimpleSSD
