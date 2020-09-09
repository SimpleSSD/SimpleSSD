// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 *         Junhyeok Jang <jhjang@camelab.org>
 */

#include "ftl/ftl.hh"

#include "ftl/allocator/basic_allocator.hh"
#include "ftl/base/basic_ftl.hh"
#include "ftl/mapping/page_level.hh"

namespace SimpleSSD {

namespace FTL {

FTL::FTL(ObjectData &o) : Object(o), requestCounter(0) {
  pFIL = new FIL::FIL(object);

  auto mapping = (Config::MappingType)readConfigUint(Section::FlashTranslation,
                                                     Config::Key::MappingMode);

  // Mapping algorithm
  switch (mapping) {
    case Config::MappingType::PageLevelFTL:
      pMapper = new Mapping::PageLevel(object);

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
      pAllocator = new BlockAllocator::BasicAllocator(object, pMapper);

      break;
  }

  // Base FTL routine
  switch (mapping) {
    default:
      pFTL = new BasicFTL(object, this, pFIL, pMapper, pAllocator);

      break;
  }
}

FTL::~FTL() {
  delete pFIL;
  delete pMapper;
  delete pAllocator;
  delete pFTL;
}

Request *FTL::insertRequest(Request &&req) {
  auto ret = requestQueue.emplace(++requestCounter, std::move(req));

  panic_if(!ret.second, "Duplicated request ID.");

  ret.first->second.tag = requestCounter;

  return &ret.first->second;
}

void FTL::initialize() {
  // Initialize all
  pAllocator->initialize(pMapper->getInfo());
  pMapper->initialize(pFTL, pAllocator);

  pFTL->initialize();
}

Parameter *FTL::getInfo() {
  return pMapper->getInfo();
}

LPN FTL::getPageUsage(LPN lpnBegin, LPN lpnEnd) {
  return pMapper->getPageUsage(lpnBegin, lpnEnd);
}

Request *FTL::getRequest(uint64_t tag) {
  auto iter = requestQueue.find(tag);

  panic_if(iter == requestQueue.end(), "Unexpected command tag %" PRIu64 ".",
           tag);

  return &iter->second;
}

void FTL::completeRequest(Request *req) {
  scheduleNow(req->event, req->data);

  auto iter = requestQueue.find(req->tag);

  requestQueue.erase(iter);
}

void FTL::read(Request &&req) {
  auto preq = insertRequest(std::move(req));

  debugprint(Log::DebugID::FTL, "READ  | LPN %" PRIu64, req.lpn);

  pFTL->read(preq);
}

void FTL::write(Request &&req) {
  auto preq = insertRequest(std::move(req));

  debugprint(Log::DebugID::FTL, "WRITE | LPN %" PRIu64, req.lpn);

  pFTL->write(preq);
}

void FTL::invalidate(Request &&req) {
  auto preq = insertRequest(std::move(req));

  debugprint(Log::DebugID::FTL, "INVAL | LPN %" PRIu64, req.lpn);

  pFTL->invalidate(preq);
}

void FTL::getStatList(std::vector<Stat> &list, std::string prefix) noexcept {
  pFTL->getStatList(list, prefix + "ftl.base.");
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
