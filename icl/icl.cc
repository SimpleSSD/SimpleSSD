// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "icl/icl.hh"

#include "hil/hil.hh"
#include "icl/cache/set_associative.hh"
#include "icl/manager/basic.hh"
#include "util/algorithm.hh"

namespace SimpleSSD::ICL {

ICL::ICL(ObjectData &o, HIL::HIL *p) : Object(o), pHIL(p) {
  pFTL = new FTL::FTL(object);
  auto *param = pFTL->getInfo();

  totalLogicalPages = param->totalLogicalPages;
  logicalPageSize = param->pageSize;

  // Create cache manager
  auto mode = (Config::Mode)readConfigUint(Section::InternalCache,
                                           Config::Key::CacheMode);

  switch (mode) {
    case Config::Mode::None:
      // pManager = new NoCache(object, this, pFTL);
      panic("NoCache not implemented.");

      break;
    case Config::Mode::SetAssociative:
      pManager = new BasicCache(object, this, pFTL);

      break;
    default:
      panic("Unexpected internal cache model.");

      break;
  }

  // Create cache structure
  switch (mode) {
    case Config::Mode::SetAssociative:
      pCache = new SetAssociative(object, pManager, param);

      break;
    default:
      panic("Unexpected internal cache model.");

      break;
  }

  // Initialize
  pManager->initialize(pCache);

  // Must be called later -- because of memory allocation
  pFTL->initialize();
}

ICL::~ICL() {
  delete pManager;
  delete pCache;
  delete pFTL;
}

void ICL::setCallbackFunction(Event e) {
  pManager->setCallbackFunction(e);
}

void ICL::read(HIL::SubRequest *req) {
  pManager->read(req);
}

void ICL::write(HIL::SubRequest *req) {
  pManager->write(req);
}

void ICL::flush(HIL::SubRequest *req) {
  pManager->flush(req);
}

void ICL::format(HIL::SubRequest *req) {
  pManager->erase(req);
}

void ICL::done(HIL::SubRequest *req) {
  pManager->dmaDone(req);
}

void ICL::makeRequest(LPN slpn, LPN elpn) {
  auto *req = new HIL::Request(InvalidEventID, 0);

  req->setAddress(slpn, elpn - slpn, logicalPageSize);

  pHIL->read(req);
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

HIL::SubRequest *ICL::getSubRequest(uint64_t tag) {
  return pHIL->getSubRequest(tag);
}

void ICL::getStatList(std::vector<Stat> &list, std::string prefix) noexcept {
  pCache->getStatList(list, prefix + "icl.cache.");
  pManager->getStatList(list, prefix + "icl.manager.");
  pFTL->getStatList(list, prefix);
}

void ICL::getStatValues(std::vector<double> &values) noexcept {
  pCache->getStatValues(values);
  pManager->getStatValues(values);
  pFTL->getStatValues(values);
}

void ICL::resetStatValues() noexcept {
  pCache->resetStatValues();
  pManager->resetStatValues();
  pFTL->resetStatValues();
}

void ICL::createCheckpoint(std::ostream &out) const noexcept {
  BACKUP_SCALAR(out, totalLogicalPages);
  BACKUP_SCALAR(out, logicalPageSize);

  pCache->createCheckpoint(out);
  pManager->createCheckpoint(out);
  pFTL->createCheckpoint(out);
}

void ICL::restoreCheckpoint(std::istream &in) noexcept {
  RESTORE_SCALAR(in, totalLogicalPages);
  RESTORE_SCALAR(in, logicalPageSize);

  pCache->restoreCheckpoint(in);
  pManager->restoreCheckpoint(in);
  pFTL->restoreCheckpoint(in);
}

HIL::SubRequest *ICL::restoreSubRequest(uint64_t tag) noexcept {
  return pHIL->restoreSubRequest(tag);
}

}  // namespace SimpleSSD::ICL
