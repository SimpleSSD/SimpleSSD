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

Request::Request(uint32_t i, uint32_t s, Event e, uint64_t d, Operation o,
                 LPN a, uint32_t sf, uint32_t se, uint8_t *b)
    : id(i),
      sid(s),
      eid(e),
      data(d),
      opcode(o),
      address(a),
      skipFront(sf),
      skipEnd(se),
      buffer(b) {}

Request::Request(uint32_t i, Event e, uint64_t d, Operation o, LPN a, LPN l)
    : id(i),
      sid(1),
      eid(e),
      data(d),
      opcode(o),
      address(a),
      length(l),
      buffer(nullptr) {}

ICL::ICL(ObjectData &o) : Object(o) {
  pFTL = new FTL::FTL(object);
  auto *param = pFTL->getInfo();

  totalLogicalPages = param->totalLogicalPages;
  logicalPageSize = param->pageSize;

  enabled =
      readConfigBoolean(Section::InternalCache, Config::Key::EnableReadCache) |
      readConfigBoolean(Section::InternalCache, Config::Key::EnableWriteCache);

  memset(&stat, 0, sizeof(stat));

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
  Stat temp;

  temp.name = prefix + "read.request_count";
  temp.desc = "Read request count";
  list.push_back(temp);

  temp.name = prefix + "read.bytes";
  temp.desc = "Read data size in byte";
  list.push_back(temp);

  temp.name = prefix + "read.busy";
  temp.desc = "Device busy time when read";
  list.push_back(temp);

  temp.name = prefix + "write.request_count";
  temp.desc = "Write request count";
  list.push_back(temp);

  temp.name = prefix + "write.bytes";
  temp.desc = "Write data size in byte";
  list.push_back(temp);

  temp.name = prefix + "write.busy";
  temp.desc = "Device busy time when write";
  list.push_back(temp);

  temp.name = prefix + "request_count";
  temp.desc = "Total request count";
  list.push_back(temp);

  temp.name = prefix + "bytes";
  temp.desc = "Total data size in byte";
  list.push_back(temp);

  temp.name = prefix + "busy";
  temp.desc = "Total device busy time";
  list.push_back(temp);

  pCache->getStatList(list, prefix + "icl.");
  pFTL->getStatList(list, prefix);
}

void ICL::getStatValues(std::vector<double> &values) noexcept {
  values.push_back(stat.request[0]);
  values.push_back(stat.iosize[0]);
  values.push_back(stat.busy[0]);
  values.push_back(stat.request[1]);
  values.push_back(stat.iosize[1]);
  values.push_back(stat.busy[1]);
  values.push_back(stat.request[0] + stat.request[1]);
  values.push_back(stat.iosize[0] + stat.iosize[1]);
  values.push_back(stat.busy[2]);

  pCache->getStatValues(values);
  pFTL->getStatValues(values);
}

void ICL::resetStatValues() noexcept {
  memset(&stat, 0, sizeof(stat));

  pCache->resetStatValues();
  pFTL->resetStatValues();
}

void ICL::createCheckpoint(std::ostream &out) const noexcept {
  BACKUP_BLOB(out, &stat, sizeof(stat));
  BACKUP_SCALAR(out, totalLogicalPages);
  BACKUP_SCALAR(out, logicalPageSize);
  BACKUP_SCALAR(out, enabled);

  pCache->createCheckpoint(out);
  pFTL->createCheckpoint(out);
}

void ICL::restoreCheckpoint(std::istream &in) noexcept {
  RESTORE_BLOB(in, &stat, sizeof(stat));
  RESTORE_SCALAR(in, totalLogicalPages);
  RESTORE_SCALAR(in, logicalPageSize);
  RESTORE_SCALAR(in, enabled);

  pCache->restoreCheckpoint(in);
  pFTL->restoreCheckpoint(in);
}

}  // namespace SimpleSSD::ICL
