// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "hil/nvme/command/abort.hh"

#include "hil/nvme/command/internal.hh"

namespace SimpleSSD::HIL::NVMe {

Abort::Abort(ObjectData &o, Subsystem *s, ControllerData *c)
    : Command(o, s, c) {
  eventAbort = createEvent([this](uint64_t) { abortDone(); },
                           "HIL::NVMe::Abort::eventAbort");
}

Abort::~Abort() {
  destroyEvent(eventAbort);
}

void Abort::abortDone() {
  data.subsystem->complete(this);
}

void Abort::setRequest(SQContext *req) {
  sqc = req;
  auto entry = sqc->getData();

  bool immediate = true;

  // Get parameters
  uint16_t sqid = entry->dword10 & 0xFFFF;
  uint16_t cid = ((entry->dword10 & 0xFFFF0000) >> 16);

  debugprint_command("ADMIN   | Abort");

  // Make response
  createResponse();

  auto ret = data.arbitrator->abortCommand(sqid, cid, eventAbort);

  switch (ret) {
    case 0:
      immediate = false;
      break;
    case 1:
    case 2:
      // Not aborted
      cqc->getData()->dword0 = 1;
      break;
  }

  if (immediate) {
    data.subsystem->complete(this);
  }
}

void Abort::getStatList(std::vector<Stat> &, std::string) noexcept {}

void Abort::getStatValues(std::vector<double> &) noexcept {}

void Abort::resetStatValues() noexcept {}

void Abort::createCheckpoint(std::ostream &out) noexcept {
  Command::createCheckpoint(out);
}

void Abort::restoreCheckpoint(std::istream &in) noexcept {
  Command::restoreCheckpoint(in);
}

}  // namespace SimpleSSD::HIL::NVMe
