// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "ftl/def.hh"

namespace SimpleSSD::FTL {

Request::Request(Event e, uint64_t d)
    : opcode(Operation::None),
      result(Response::Success),
      lpn(InvalidLPN),
      ppn(InvalidPPN),
      slpn(0),
      nlp(0),
      event(e),
      data(d),
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

  lpn = r->getLPN();
  slpn = r->getSLPN();
  nlp = r->getNLP();
  data = r->getTag();
}

void Request::createCheckpoint(std::ostream &out) const noexcept {
  BACKUP_SCALAR(out, tag);
  BACKUP_SCALAR(out, opcode);
  BACKUP_SCALAR(out, result);
  BACKUP_SCALAR(out, lpn);
  BACKUP_SCALAR(out, ppn);
  BACKUP_SCALAR(out, slpn);
  BACKUP_SCALAR(out, nlp);
  BACKUP_EVENT(out, event);
  BACKUP_SCALAR(out, data);
}

void Request::restoreCheckpoint(std::istream &in, ObjectData &object) noexcept {
  RESTORE_SCALAR(in, tag);
  RESTORE_SCALAR(in, opcode);
  RESTORE_SCALAR(in, result);
  RESTORE_SCALAR(in, lpn);
  RESTORE_SCALAR(in, ppn);
  RESTORE_SCALAR(in, slpn);
  RESTORE_SCALAR(in, nlp);
  RESTORE_EVENT(in, event);
  RESTORE_SCALAR(in, data);
}

}  // namespace SimpleSSD::FTL
