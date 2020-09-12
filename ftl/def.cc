// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 *         Junhyeok Jang <jhjang@camelab.org>
 */

#include "ftl/def.hh"

#include "ftl/base/abstract_ftl.hh"

namespace SimpleSSD::FTL {

Request::Request(Event e, uint64_t d)
    : offset(0),
      length(0),
      slpn(0),
      nlp(0),
      opcode(Operation::None),
      result(Response::Success),
      event(e),
      data(d),
      address(0),
      counter(0) {}

Request::Request(Event e, HIL::SubRequest *r)
    : result(Response::Success), event(e), counter(0) {
  // Opcode
  switch (r->getOpcode()) {
    case HIL::Operation::Read:
      opcode = Operation::Read;

      break;
    case HIL::Operation::Write:
    case HIL::Operation::WriteZeroes:
      opcode = Operation::Write;

      break;
    case HIL::Operation::Trim:
      opcode = Operation::Trim;

      break;
    case HIL::Operation::Format:
      opcode = Operation::Format;

      break;
    default:
      // TODO: Panic here
      break;
  }

  offset = (uint32_t)r->getOffset();
  length = r->getLength();
  lpn = r->getLPN();
  slpn = r->getSLPN();
  nlp = r->getNLP();
  data = r->getTag();
  address = r->getDRAMAddress();
}

Request::Request(Operation op, LPN l, uint32_t o, uint32_t s, LPN sl,
                 uint32_t nl, Event e, uint64_t d)
    : lpn(l),
      offset(o),
      length(s),
      slpn(sl),
      nlp(nl),
      opcode(op),
      result(Response::Success),
      event(e),
      data(d),
      address(0),
      counter(0) {}

Request::Request(PPN p)
    : ppn(p),
      offset(0),
      length(0),
      slpn(0),
      nlp(0),
      opcode(Operation::None),
      result(Response::Success),
      event(InvalidEventID),
      data(0),
      address(0),
      counter(0) {}

void Request::createCheckpoint(std::ostream &out) const noexcept {
  BACKUP_SCALAR(out, tag);
  BACKUP_SCALAR(out, opcode);
  BACKUP_SCALAR(out, result);
  BACKUP_SCALAR(out, lpn);
  BACKUP_SCALAR(out, ppn);
  BACKUP_SCALAR(out, offset);
  BACKUP_SCALAR(out, length);
  BACKUP_SCALAR(out, slpn);
  BACKUP_SCALAR(out, nlp);
  BACKUP_EVENT(out, event);
  BACKUP_SCALAR(out, data);
  BACKUP_SCALAR(out, address);
  BACKUP_SCALAR(out, counter);
}

void Request::restoreCheckpoint(std::istream &in, ObjectData &object) noexcept {
  RESTORE_SCALAR(in, tag);
  RESTORE_SCALAR(in, opcode);
  RESTORE_SCALAR(in, result);
  RESTORE_SCALAR(in, lpn);
  RESTORE_SCALAR(in, ppn);
  RESTORE_SCALAR(in, offset);
  RESTORE_SCALAR(in, length);
  RESTORE_SCALAR(in, slpn);
  RESTORE_SCALAR(in, nlp);
  RESTORE_EVENT(in, event);
  RESTORE_SCALAR(in, data);
  RESTORE_SCALAR(in, address);
  RESTORE_SCALAR(in, counter);
}

void backupSuperRequest(std::ostream &out, const SuperRequest &list) noexcept {
  bool exist;
  uint64_t size = list.size();
  BACKUP_SCALAR(out, size);

  for (auto &iter : list) {
    exist = iter != nullptr;

    BACKUP_SCALAR(out, exist);

    if (exist) {
      uint64_t tag = iter->getTag();

      BACKUP_SCALAR(out, tag);
    }
  }
}

void restoreSuperRequest(std::istream &in, ObjectData &object,
                         SuperRequest &list, AbstractFTL *p) noexcept {
  bool exist;
  uint64_t size;

  RESTORE_SCALAR(in, size);

  uint64_t tag;

  for (uint64_t i = 0; i < size; i++) {
    RESTORE_SCALAR(in, exist);

    if (exist) {
      RESTORE_SCALAR(in, tag);

      list.emplace_back(p->getRequest(tag));
    }
  }
}

void ReadModifyWriteContext::createCheckpoint(std::ostream &out) const
    noexcept {
  // next and last should be handled outside of this function
  BACKUP_SCALAR(out, alignedBegin);
  BACKUP_SCALAR(out, chunkBegin);

  backupSuperRequest(out, list);

  BACKUP_SCALAR(out, writePending);
  BACKUP_SCALAR(out, counter);
  BACKUP_SCALAR(out, beginAt);
}

void ReadModifyWriteContext::restoreCheckpoint(std::istream &in,
                                               ObjectData &object,
                                               AbstractFTL *p) noexcept {
  RESTORE_SCALAR(in, alignedBegin);
  RESTORE_SCALAR(in, chunkBegin);

  restoreSuperRequest(in, object, list, p);

  RESTORE_SCALAR(in, writePending);
  RESTORE_SCALAR(in, counter);
  RESTORE_SCALAR(in, beginAt);
}

CopyContext::~CopyContext() {
  releaseList();
}

void CopyContext::reset() {
  releaseList();
  list.clear();
  tag2ListIdx.clear();
  readCounter = 0;
  writeCounter.clear();
  copyCounter = 0;
  eraseCounter = 0;
  beginAt = 0;
}

void CopyContext::releaseList() {
  for (auto sReq : list) {
    for (auto req : sReq) {
      delete req;
    }
  }
}

CopyContext &CopyContext::operator=(CopyContext &&rhs) {
  sblockID = rhs.sblockID;
  std::swap(list, rhs.list);
  iter = std::move(rhs.iter);
  tag2ListIdx = std::move(rhs.tag2ListIdx);
  readCounter = rhs.readCounter;
  writeCounter = std::move(rhs.writeCounter);
  beginAt = rhs.beginAt;

  return *this;
}

}  // namespace SimpleSSD::FTL
