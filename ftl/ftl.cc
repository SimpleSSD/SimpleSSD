// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "ftl/ftl.hh"

#include "ftl/allocator/basic_allocator.hh"
#include "ftl/base/basic_ftl.hh"

namespace SimpleSSD {

namespace FTL {

FTL::FTL(ObjectData &o) : Object(o) {
  pFIL = new FIL::FIL(object);

  auto mapping = (Config::MappingType)readConfigUint(Section::FlashTranslation,
                                                     Config::Key::MappingMode);

  // Mapping algorithm
  switch (mapping) {
    case Config::MappingType::PageLevelFTL:
      // pMapper = new Mapping::PageLevel(object);

      break;
    // case Config::MappingType::BlockLevelFTL:
    //   pMapper = new Mapping::BlockLevel(object);

    //   break;
    default:
      panic("Unsupported mapping type.");

      break;
  }

  // Block allocator
  switch (mapping) {
    default:
      // pAllocator = new BlockAllocator::BasicAllocator(object, pMapper);

      break;
  }

  // Base FTL routine
  switch (mapping) {
    default:
      // pFTL = new BasicFTL(object, pFIL, pMapper, pAllocator);

      break;
  }

  // Initialize all
  pAllocator->initialize(pMapper->getInfo());
  pMapper->initialize(pFTL, pAllocator);

  pFTL->initialize();
}

FTL::~FTL() {
  delete pFIL;
  delete pMapper;
  delete pAllocator;
  delete pFTL;
}

Parameter *FTL::getInfo() {
  return pMapper->getInfo();
}

uint32_t FTL::getMappingGranularity() {
  return (uint32_t)pMapper->mappingGranularity();
}

LPN FTL::getPageUsage(LPN lpnBegin, LPN lpnEnd) {
  return pMapper->getPageUsage(lpnBegin, lpnEnd);
}

void FTL::read(Request &req) {
  pFTL->read(req);
}

void FTL::write(Request &req) {
  pFTL->write(req);
}

void FTL::invalidate(LPN lpn, uint32_t nlp, Event eid, uint64_t data) {
  pFTL->invalidate(lpn, nlp, eid, data);
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
