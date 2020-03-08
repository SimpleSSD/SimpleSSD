// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "hil/request.hh"

#include "hil/hil.hh"

namespace SimpleSSD::HIL {

static const char *OperationName[] = {
    "NOP    ",  // None
    "READ   ",  // Read
    "WRITE  ",  // Write
    "WRZERO ",  // WriteZeroes
    "COMPARE",  // Compare
    "CMP+WR ",  // CompareAndWrite
    "FLUSH  ",  // Flush
    "TRIM   ",  // Trim
    "FORMAT ",  // Format
};

const char *getOperationName(Operation op) {
  return OperationName[(uint16_t)op];
}

Request::Request()
    : opcode(Operation::None),
      result(Response::Success),
      lbaSize(0),
      dmaEngine(nullptr),
      dmaTag(InvalidDMATag),
      eid(InvalidEventID),
      data(0),
      offset(0),
      length(0),
      dmaCounter(0),
      nvmCounter(0),
      nlp(0),
      dmaBeginAt(0),
      nvmBeginAt(0),
      requestTag(0),
      slpn(0) {}

Request::Request(Event e, uint64_t c)
    : opcode(Operation::None),
      result(Response::Success),
      lbaSize(0),
      dmaEngine(nullptr),
      dmaTag(InvalidDMATag),
      eid(e),
      data(c),
      offset(0),
      length(0),
      dmaCounter(0),
      nvmCounter(0),
      nlp(0),
      dmaBeginAt(0),
      nvmBeginAt(0),
      requestTag(0),
      slpn(0) {}

void Request::createCheckpoint(std::ostream &out) const noexcept {
  BACKUP_SCALAR(out, opcode);
  BACKUP_SCALAR(out, result);
  BACKUP_SCALAR(out, lbaSize);
  BACKUP_EVENT(out, eid);
  BACKUP_SCALAR(out, data);
  BACKUP_SCALAR(out, offset);
  BACKUP_SCALAR(out, length);
  BACKUP_SCALAR(out, dmaCounter);
  BACKUP_SCALAR(out, nvmCounter);
  BACKUP_SCALAR(out, nlp);
  BACKUP_SCALAR(out, dmaBeginAt);
  BACKUP_SCALAR(out, nvmBeginAt);
  BACKUP_SCALAR(out, requestTag);
  BACKUP_SCALAR(out, slpn);
}

void Request::restoreCheckpoint(std::istream &in, ObjectData &object) noexcept {
  RESTORE_SCALAR(in, opcode);
  RESTORE_SCALAR(in, result);
  RESTORE_SCALAR(in, lbaSize);
  RESTORE_EVENT(in, eid);
  RESTORE_SCALAR(in, data);
  RESTORE_SCALAR(in, offset);
  RESTORE_SCALAR(in, length);
  RESTORE_SCALAR(in, dmaCounter);
  RESTORE_SCALAR(in, nvmCounter);
  RESTORE_SCALAR(in, nlp);
  RESTORE_SCALAR(in, dmaBeginAt);
  RESTORE_SCALAR(in, nvmBeginAt);
  RESTORE_SCALAR(in, requestTag);
  RESTORE_SCALAR(in, slpn);
}

SubRequest::SubRequest()
    : requestTag(0),
      request(nullptr),
      lpn(InvalidLPN),
      offset(0),
      length(0),
      allocate(false),
      clear(false),
      skipFront(0),
      skipEnd(0),
      buffer(nullptr),
      address(0) {}

SubRequest::SubRequest(uint64_t t, Request *r)
    : requestTag(t),
      request(r),
      lpn(InvalidLPN),
      offset(0),
      length(0),
      allocate(false),
      clear(false),
      skipFront(0),
      skipEnd(0),
      buffer(nullptr),
      address(0) {}

SubRequest::SubRequest(uint64_t t, Request *r, LPN l, uint64_t o, uint32_t s)
    : requestTag(t),
      request(r),
      lpn(l),
      offset(o),
      length(s),
      allocate(false),
      clear(false),
      skipFront(0),
      skipEnd(0),
      buffer(nullptr),
      address(0) {}

void SubRequest::createCheckpoint(std::ostream &out) const noexcept {
  BACKUP_SCALAR(out, requestTag);

  uint64_t tag = request->getTag();
  BACKUP_SCALAR(out, tag);

  BACKUP_SCALAR(out, lpn);
  BACKUP_SCALAR(out, offset);
  BACKUP_SCALAR(out, length);
  BACKUP_SCALAR(out, allocate);
  BACKUP_SCALAR(out, clear);
  BACKUP_SCALAR(out, skipFront);
  BACKUP_SCALAR(out, skipEnd);

  if (clear) {
    BACKUP_BLOB(out, buffer, length);
  }
  else {
    BACKUP_SCALAR(out, buffer);
  }

  BACKUP_SCALAR(out, address);
}

void SubRequest::restoreCheckpoint(std::istream &in, HIL *pHIL) noexcept {
  RESTORE_SCALAR(in, requestTag);

  uint64_t tag;
  RESTORE_SCALAR(in, tag);

  request = pHIL->restoreRequest(tag);

  RESTORE_SCALAR(in, lpn);
  RESTORE_SCALAR(in, offset);
  RESTORE_SCALAR(in, length);
  RESTORE_SCALAR(in, allocate);
  RESTORE_SCALAR(in, clear);
  RESTORE_SCALAR(in, skipFront);
  RESTORE_SCALAR(in, skipEnd);

  if (clear) {
    buffer = (uint8_t *)calloc(length, 1);

    RESTORE_BLOB(in, buffer, length);
  }
  else {
    RESTORE_SCALAR(in, buffer);
  }

  RESTORE_SCALAR(in, address);
}

}  // namespace SimpleSSD::HIL
