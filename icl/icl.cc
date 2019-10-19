// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "icl/icl.hh"

#include "icl/ring_buffer.hh"
#include "util/algorithm.hh"

namespace SimpleSSD::ICL {

ICL::ICL(ObjectData &o, HIL::CommandManager *m) : Object(o), commandManager(m) {
  pFTL = new FTL::FTL(object);
  auto *param = pFTL->getInfo();

  totalLogicalPages = param->totalLogicalPages;
  logicalPageSize = param->pageSize;

  switch ((Config::Mode)readConfigUint(Section::InternalCache,
                                       Config::Key::CacheMode)) {
    case Config::Mode::RingBuffer:
      pCache = new RingBuffer(object, commandManager, pFTL);

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

void ICL::submit(uint64_t tag, uint32_t id) {
  pCache->enqueue(tag, id);
}

LPN ICL::getPageUsage(LPN offset, LPN length) {
  return pFTL->getPageUsage(offset, length);
}

LPN ICL::getTotalPages() {
  return totalLogicalPages;
}

uint64_t ICL::getLPNSize() {
  return logicalPageSize;
}

void ICL::setCache(bool set) {
  pCache->setCache(set);
}

bool ICL::getCache() {
  return pCache->getCache();
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
