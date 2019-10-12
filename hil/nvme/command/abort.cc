// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "hil/nvme/command/abort.hh"

#include "hil/nvme/command/internal.hh"

namespace SimpleSSD::HIL::NVMe {

Abort::Abort(ObjectData &o, Subsystem *s) : Command(o, s) {
  eventAbort = createEvent([this](uint64_t, uint64_t gcid) { abortDone(gcid); },
                           "HIL::NVMe::Abort::eventAbort");
}

void Abort::abortDone(uint64_t gcid) {
  auto tag = findTag(gcid);

  subsystem->complete(tag);
}

void Abort::setRequest(ControllerData *cdata, SQContext *req) {
  auto tag = createTag(cdata, req);
  auto entry = tag->sqc->getData();

  bool immediate = true;

  // Get parameters
  uint16_t sqid = entry->dword10 & 0xFFFF;
  uint16_t cid = ((entry->dword10 & 0xFFFF0000) >> 16);

  debugprint_command(tag, "ADMIN   | Abort");

  // Make response
  tag->createResponse();

  auto ret =
      tag->arbitrator->abortCommand(sqid, cid, eventAbort, tag->getGCID());

  switch (ret) {
    case 0:
      immediate = false;
      break;
    case 1:
    case 2:
      // Not aborted
      tag->cqc->getData()->dword0 = 1;

      break;
  }

  if (immediate) {
    subsystem->complete(tag);
  }
}

void Abort::getStatList(std::vector<Stat> &, std::string) noexcept {}

void Abort::getStatValues(std::vector<double> &) noexcept {}

void Abort::resetStatValues() noexcept {}

void Abort::createCheckpoint(std::ostream &out) const noexcept {
  Command::createCheckpoint(out);

  BACKUP_EVENT(out, eventAbort);
}

void Abort::restoreCheckpoint(std::istream &in) noexcept {
  Command::restoreCheckpoint(in);

  RESTORE_EVENT(in, eventAbort);
}

}  // namespace SimpleSSD::HIL::NVMe
