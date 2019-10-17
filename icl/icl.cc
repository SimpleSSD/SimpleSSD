// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "icl/icl.hh"

#include "icl/set_associative.hh"
#include "util/algorithm.hh"

namespace SimpleSSD::ICL {

Request::Request()
    : id(0),
      eid(InvalidEventID),
      data(0),
      address(0),
      length(0),
      buffer(nullptr) {}

Request::Request(uint64_t i, Event e, uint64_t d, Operation o, LPN a,
                 uint32_t sf, uint32_t se, uint8_t *b)
    : id(i),
      eid(e),
      data(d),
      opcode(o),
      address(a),
      skipFront(sf),
      skipEnd(se),
      buffer(b) {}

Request::Request(uint64_t i, Event e, uint64_t d, Operation o, LPN a, LPN l)
    : id(i),
      eid(e),
      data(d),
      opcode(o),
      address(a),
      length(l),
      buffer(nullptr) {}

void Request::backup(std::ostream &out) const {
  BACKUP_SCALAR(out, id);
  BACKUP_EVENT(out, eid);
  BACKUP_SCALAR(out, data);
  BACKUP_SCALAR(out, opcode);
  BACKUP_SCALAR(out, address);
  BACKUP_SCALAR(out, length);
  BACKUP_SCALAR(out, buffer);
}

void Request::restore(ObjectData &object, std::istream &in) {
  RESTORE_SCALAR(in, id);
  RESTORE_EVENT(in, eid);
  RESTORE_SCALAR(in, data);
  RESTORE_SCALAR(in, opcode);
  RESTORE_SCALAR(in, address);
  RESTORE_SCALAR(in, length);
  RESTORE_SCALAR(in, buffer);

  buffer = object.bufmgr->restorePointer(buffer);
}

ICL::ICL(ObjectData &o) : Object(o) {
  pFTL = new FTL::FTL(object);
  auto *param = pFTL->getInfo();

  totalLogicalPages = param->totalLogicalPages;
  logicalPageSize = param->pageSize;

  enabled =
      readConfigBoolean(Section::InternalCache, Config::Key::EnableReadCache) |
      readConfigBoolean(Section::InternalCache, Config::Key::EnableWriteCache);

  switch ((Config::Mode)readConfigUint(Section::InternalCache,
                                       Config::Key::CacheMode)) {
    case Config::Mode::SetAssociative:
      pCache = new SetAssociative(object, pFTL);

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

void ICL::submit(Request &&req) {
  pCache->enqueue(std::move(req));
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
  enabled = set;
}

bool ICL::getCache() {
  return enabled;
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
  BACKUP_SCALAR(out, enabled);

  pCache->createCheckpoint(out);
  pFTL->createCheckpoint(out);
}

void ICL::restoreCheckpoint(std::istream &in) noexcept {
  RESTORE_SCALAR(in, totalLogicalPages);
  RESTORE_SCALAR(in, logicalPageSize);
  RESTORE_SCALAR(in, enabled);

  pCache->restoreCheckpoint(in);
  pFTL->restoreCheckpoint(in);
}

}  // namespace SimpleSSD::ICL
