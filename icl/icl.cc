// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "icl/icl.hh"

#include "icl/abstract_cache.hh"
#include "util/algorithm.hh"

namespace SimpleSSD::ICL {

ICL::ICL(ObjectData &o) : Object(o), eventHILCompletion(InvalidEventID) {
  auto *param = pFTL->getInfo();

  totalLogicalPages = param->totalLogicalPages;
  logicalPageSize = param->pageSize;

  enabled = readConfigBoolean(Section::InternalCache, Config::Key::EnableCache);

  switch ((Config::Mode)readConfigUint(Section::InternalCache,
                                       Config::Key::CacheMode)) {
    case Config::Mode::RingBuffer:
      // pCache = new RingBuffer(object, commandManager, pFTL);

      break;
    default:
      panic("Unexpected internal cache model.");

      break;
  }
}

ICL::~ICL() {
  delete pCache;
  delete pFTL;
}

void ICL::setCallbackFunction(Event e) {
  eventHILCompletion = e;
}

void ICL::submit(SubRequest *req) {
  if (LIKELY(enabled)) {
    pCache->submit(req);
  }
  else {
    pFTL->submit(req);
  }
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
