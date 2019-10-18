// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "hil/hil.hh"

namespace SimpleSSD::HIL {

HIL::HIL(ObjectData &o) : Object(o), commandManager(object), requestCounter(0) {
  pICL = new ICL::ICL(object);

  logicalPageSize = pICL->getLPNSize();
}

HIL::~HIL() {
  delete pICL;
}

void HIL::submitCommand(uint64_t tag) {
  pICL->submit(tag);
}

LPN HIL::getPageUsage(LPN offset, LPN length) {
  return pICL->getPageUsage(offset, length);
}

LPN HIL::getTotalPages() {
  return pICL->getTotalPages();
}

uint64_t HIL::getLPNSize() {
  return logicalPageSize;
}

void HIL::setCache(bool set) {
  pICL->setCache(set);
}

bool HIL::getCache() {
  return pICL->getCache();
}

void HIL::getStatList(std::vector<Stat> &list, std::string prefix) noexcept {
  pICL->getStatList(list, prefix);
}

void HIL::getStatValues(std::vector<double> &values) noexcept {
  pICL->getStatValues(values);
}

void HIL::resetStatValues() noexcept {
  pICL->resetStatValues();
}

void HIL::createCheckpoint(std::ostream &out) const noexcept {
  BACKUP_SCALAR(out, requestCounter);

  commandManager.createCheckpoint(out);
  pICL->createCheckpoint(out);
}

void HIL::restoreCheckpoint(std::istream &in) noexcept {
  RESTORE_SCALAR(in, requestCounter);

  commandManager.restoreCheckpoint(in);
  pICL->restoreCheckpoint(in);

  // Just update this value here
  logicalPageSize = pICL->getLPNSize();
}

}  // namespace SimpleSSD::HIL
