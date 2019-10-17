// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "hil/hil.hh"

namespace SimpleSSD::HIL {

HIL::HIL(ObjectData &o) : Object(o), requestCounter(0) {
  pICL = new ICL::ICL(object);

  logicalPageSize = pICL->getLPNSize();

  memset(&stat, 0, sizeof(stat));
}

HIL::~HIL() {
  delete pICL;
}

void HIL::readPage(LPN address, uint8_t *buffer,
                   std::pair<uint32_t, uint32_t> &&unread, Event eid,
                   uint64_t data) {
  stat.request[StatRead]++;
  stat.iosize[StatRead] += logicalPageSize - unread.first - unread.second;

  pICL->submit(ICL::Request(requestCounter++, eid, data, ICL::Operation::Read,
                            address, unread.first, unread.second, buffer));
}

void HIL::writePage(LPN address, uint8_t *buffer,
                    std::pair<uint32_t, uint32_t> &&unwritten, Event eid,
                    uint64_t data) {
  stat.request[StatWrite]++;
  stat.iosize[StatWrite] +=
      logicalPageSize - unwritten.first - unwritten.second;

  pICL->submit(ICL::Request(requestCounter++, eid, data, ICL::Operation::Write,
                            address, unwritten.first, unwritten.second,
                            buffer));
}

void HIL::flushCache(LPN offset, LPN length, Event eid, uint64_t data) {
  pICL->submit(ICL::Request(requestCounter++, eid, data, ICL::Operation::Flush,
                            offset, length));
}

void HIL::trimPages(LPN offset, LPN length, Event eid, uint64_t data) {
  pICL->submit(ICL::Request(requestCounter++, eid, data, ICL::Operation::Trim,
                            offset, length));
}

void HIL::formatPages(LPN offset, LPN length, FormatOption option, Event eid,
                      uint64_t data) {
  switch (option) {
    case FormatOption::None:
      pICL->submit(ICL::Request(requestCounter++, eid, data,
                                ICL::Operation::Trim, offset, length));
      break;
    case FormatOption::UserDataErase:  // Same with FormatOption::BlockErase:
      pICL->submit(ICL::Request(requestCounter++, eid, data,
                                ICL::Operation::Format, offset, length));
      break;
    default:
      panic("Unsupported format option specified.");
      break;
  }
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
  Stat temp;

  temp.name = prefix + "read.request_count";
  temp.desc = "Read request count";
  list.push_back(temp);

  temp.name = prefix + "read.bytes";
  temp.desc = "Read data size in byte";
  list.push_back(temp);

  temp.name = prefix + "write.request_count";
  temp.desc = "Write request count";
  list.push_back(temp);

  temp.name = prefix + "write.bytes";
  temp.desc = "Write data size in byte";
  list.push_back(temp);

  temp.name = prefix + "request_count";
  temp.desc = "Total request count";
  list.push_back(temp);

  pICL->getStatList(list, prefix);
}

void HIL::getStatValues(std::vector<double> &values) noexcept {
  values.push_back(stat.request[0]);
  values.push_back(stat.iosize[0]);
  values.push_back(stat.request[1]);
  values.push_back(stat.iosize[1]);
  values.push_back(stat.request[0] + stat.request[1]);

  pICL->getStatValues(values);
}

void HIL::resetStatValues() noexcept {
  memset(&stat, 0, sizeof(stat));

  pICL->resetStatValues();
}

void HIL::createCheckpoint(std::ostream &out) const noexcept {
  BACKUP_SCALAR(out, requestCounter);
  BACKUP_BLOB(out, &stat, sizeof(stat));

  pICL->createCheckpoint(out);
}

void HIL::restoreCheckpoint(std::istream &in) noexcept {
  RESTORE_SCALAR(in, requestCounter);
  RESTORE_BLOB(in, &stat, sizeof(stat));

  pICL->restoreCheckpoint(in);

  // Just update this value here
  logicalPageSize = pICL->getLPNSize();
}

}  // namespace SimpleSSD::HIL
