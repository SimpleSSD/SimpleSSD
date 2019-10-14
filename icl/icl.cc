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

Request::Request()
    : id(0),
      sid(0),
      eid(InvalidEventID),
      data(0),
      address(0),
      skipFront(0),
      skipEnd(0),
      buffer(nullptr) {}

Request::Request(uint64_t i, uint64_t s, Event e, uint64_t d, uint64_t a,
                 uint32_t sf, uint32_t se, uint8_t *b)
    : id(i),
      sid(s),
      eid(e),
      data(d),
      address(a),
      skipFront(sf),
      skipEnd(se),
      buffer(b) {}

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
      // pCache = new GenericCache(conf, pFTL);

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
  // Enqueue
  pendingQueue.emplace_back(req);

  // TODO: Trigger cache
}

void ICL::submit(FTL::Request &&req) {
  // TODO: Pass request to FTL
}

LPN ICL::getPageUsage(LPN, LPN) {
  // TODO: Pass to FTL
  return 0;
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
  bool exist;

  BACKUP_SCALAR(out, totalLogicalPages);
  BACKUP_SCALAR(out, logicalPageSize);
  BACKUP_SCALAR(out, enabled);

  uint64_t size = pendingQueue.size();
  BACKUP_SCALAR(out, size);

  for (auto &iter : pendingQueue) {
    BACKUP_SCALAR(out, iter.id);
    BACKUP_SCALAR(out, iter.sid);
    BACKUP_EVENT(out, iter.eid);
    BACKUP_SCALAR(out, iter.data);
    BACKUP_SCALAR(out, iter.address);
    BACKUP_SCALAR(out, iter.skipFront);
    BACKUP_SCALAR(out, iter.skipEnd);

    exist = iter.buffer != nullptr;
    BACKUP_SCALAR(out, exist);

    if (exist) {
      BACKUP_BLOB(out, iter.buffer, logicalPageSize);
    }
  }

  pCache->createCheckpoint(out);
  pFTL->createCheckpoint(out);
}

void ICL::restoreCheckpoint(std::istream &in) noexcept {
  bool exist;

  RESTORE_SCALAR(in, totalLogicalPages);
  RESTORE_SCALAR(in, logicalPageSize);
  RESTORE_SCALAR(in, enabled);

  uint64_t size;
  RESTORE_SCALAR(in, size);

  for (uint64_t i = 0; i < size; i++) {
    Request tmp;

    RESTORE_SCALAR(in, tmp.id);
    RESTORE_SCALAR(in, tmp.sid);
    RESTORE_EVENT(in, tmp.eid);
    RESTORE_SCALAR(in, tmp.data);
    RESTORE_SCALAR(in, tmp.address);
    RESTORE_SCALAR(in, tmp.skipFront);
    RESTORE_SCALAR(in, tmp.skipEnd);

    RESTORE_SCALAR(in, exist);

    if (exist) {
      RESTORE_BLOB(in, tmp.buffer, logicalPageSize);
    }
  }

  pCache->restoreCheckpoint(in);
  pFTL->restoreCheckpoint(in);
}

}  // namespace SimpleSSD::ICL
