// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "icl/icl.hh"

#include "icl/cache/abstract_cache.hh"
#include "icl/manager/abstract_manager.hh"
#include "util/algorithm.hh"

namespace SimpleSSD::ICL {

ICL::ICL(ObjectData &o) : Object(o), eventHILCompletion(InvalidEventID) {
  auto *param = pFTL->getInfo();

  totalLogicalPages = param->totalLogicalPages;
  logicalPageSize = param->pageSize;

  // Create cache manager
  auto mode = (Config::Mode)readConfigUint(Section::InternalCache,
                                           Config::Key::CacheMode);

  switch (mode) {
    case Config::Mode::None:
      break;
    case Config::Mode::RingBuffer:
    case Config::Mode::SetAssociative:
      break;
    default:
      panic("Unexpected internal cache model.");

      break;
  }

  // Create cache structure
  switch (mode) {
    case Config::Mode::RingBuffer:
      // pCache = new RingBuffer(object, commandManager, pFTL);

      break;
    default:
      panic("Unexpected internal cache model.");

      break;
  }

  // Initialize
  pManager->initialize(pCache);
}

ICL::~ICL() {
  delete pManager;
  delete pCache;
  delete pFTL;
}

void ICL::setCallbackFunction(Event e) {
  eventHILCompletion = e;
}

void ICL::read(SubRequest *req) {
  pManager->read(req);
}

void ICL::write(SubRequest *req) {
  pManager->write(req);
}

void ICL::flush(SubRequest *req) {
  pManager->flush(req);
}

void ICL::format(SubRequest *req) {
  pManager->erase(req);
}

void ICL::done(SubRequest *req) {
  pManager->dmaDone(req);
}

LPN ICL::getPageUsage(LPN offset, LPN length) {
  return pFTL->getPageUsage(offset, length);
}

LPN ICL::getTotalPages() {
  return totalLogicalPages;
}

uint32_t ICL::getLPNSize() {
  return logicalPageSize;
}

void ICL::getStatList(std::vector<Stat> &list, std::string prefix) noexcept {
  pCache->getStatList(list, prefix + "icl.");
  pFTL->getStatList(list, prefix);
}

void ICL::getStatValues(std::vector<double> &values) noexcept {
  pCache->getStatValues(values);
  pFTL->getStatValues(values);
}

void ICL::resetStatValues() noexcept {
  pCache->resetStatValues();
  pFTL->resetStatValues();
}

void ICL::createCheckpoint(std::ostream &out) const noexcept {
  BACKUP_SCALAR(out, totalLogicalPages);
  BACKUP_SCALAR(out, logicalPageSize);

  pCache->createCheckpoint(out);
  pFTL->createCheckpoint(out);
}

void ICL::restoreCheckpoint(std::istream &in) noexcept {
  RESTORE_SCALAR(in, totalLogicalPages);
  RESTORE_SCALAR(in, logicalPageSize);

  pCache->restoreCheckpoint(in);
  pFTL->restoreCheckpoint(in);
}

}  // namespace SimpleSSD::ICL
