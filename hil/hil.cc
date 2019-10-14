// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "hil/hil.hh"

namespace SimpleSSD::HIL {

HIL::HIL(ObjectData &o) : Object(o), requestCounter(1) {
  pICL = new ICL::ICL(object);

  logicalPageSize = pICL->getLPNSize();
}

HIL::~HIL() {
  delete pICL;
}

void HIL::readPages(LPN offset, LPN length, uint8_t *buffer,
                    std::pair<uint32_t, uint32_t> &&unread, Event eid,
                    uint64_t data) {
  uint64_t subID = 1;
  uint32_t skipFirst = unread.first;
  uint32_t skipEnd = 0;

  // When reading, we don't need to care about skip bytes
  // But for statistic (host read bytes), pass skip bytes to ICL
  for (LPN i = 0; i < length; i++) {
    if (i == length - 1) {
      skipEnd = unread.second;
    }

    pICL->submit(ICL::Request(
        requestCounter++, subID++, eid, data, ICL::Operation::Read, offset + i,
        skipFirst, skipEnd,
        buffer == nullptr ? nullptr : buffer + i * logicalPageSize));

    if (i == 0) {
      skipFirst = 0;
    }
  }
}

void HIL::writePages(LPN offset, LPN length, uint8_t *buffer,
                     std::pair<uint32_t, uint32_t> &&unwritten, Event eid,
                     uint64_t data) {
  uint64_t subID = 1;
  uint32_t skipFirst = unwritten.first;
  uint32_t skipEnd = 0;

  // Only first/last request has skip bytes
  for (LPN i = 0; i < length; i++) {
    if (i == length - 1) {
      skipEnd = unwritten.second;
    }

    pICL->submit(ICL::Request(
        requestCounter++, subID++, eid, data, ICL::Operation::Write, offset + i,
        skipFirst, skipEnd,
        buffer == nullptr ? nullptr : buffer + i * logicalPageSize));

    if (i == 0) {
      skipFirst = 0;
    }
  }
}

void HIL::flushCache(LPN offset, LPN length, Event eid, uint64_t data) {
  pICL->submit(ICL::Request(requestCounter++, 1, eid, data,
                            ICL::Operation::Flush, offset, length));
}

void HIL::trimPages(LPN offset, LPN length, Event eid, uint64_t data) {
  pICL->submit(ICL::Request(requestCounter++, 1, eid, data,
                            ICL::Operation::Trim, offset, length));
}

void HIL::formatPages(LPN offset, LPN length, FormatOption option, Event eid,
                      uint64_t data) {
  switch (option) {
    case FormatOption::None:
      pICL->submit(ICL::Request(requestCounter++, 1, eid, data,
                                ICL::Operation::Trim, offset, length));
      break;
    case FormatOption::UserDataErase:  // Same with FormatOption::BlockErase:
      pICL->submit(ICL::Request(requestCounter++, 1, eid, data,
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

  pICL->createCheckpoint(out);
}

void HIL::restoreCheckpoint(std::istream &in) noexcept {
  RESTORE_SCALAR(in, requestCounter);

  pICL->restoreCheckpoint(in);

  // Just update this value here
  logicalPageSize = pICL->getLPNSize();
}

}  // namespace SimpleSSD::HIL
