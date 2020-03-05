// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "ftl/def.hh"

namespace SimpleSSD::FTL {

Request::Request(Event e, uint64_t d)
    : lpn(InvalidLPN),
      ppn(InvalidPPN),
      offset(0),
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
    : result(Response::Success), ppn(InvalidPPN), event(e), counter(0) {
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
      ppn(InvalidPPN),
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

}  // namespace SimpleSSD::FTL
