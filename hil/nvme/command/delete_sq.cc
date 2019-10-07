// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "hil/nvme/command/delete_sq.hh"

#include "hil/nvme/command/internal.hh"

namespace SimpleSSD::HIL::NVMe {

DeleteSQ::DeleteSQ(ObjectData &o, Subsystem *s, ControllerData *c)
    : Command(o, s, c) {
  eventErased = createEvent([this](uint64_t) { eraseDone(); },
                            "HIL::NVMe::DeleteSQ::eventErased");
}

DeleteSQ::~DeleteSQ() {
  destroyEvent(eventErased);
}

void DeleteSQ::eraseDone() {
  data.subsystem->complete(this);
}

void DeleteSQ::setRequest(SQContext *req) {
  sqc = req;
  auto entry = sqc->getData();

  bool immediate = true;

  // Get parameters
  uint16_t id = entry->dword10 & 0xFFFF;

  debugprint_command("ADMIN   | Delete I/O Submission Queue");

  // Make response
  createResponse();

  auto ret = data.arbitrator->deleteIOSQ(id, eventErased);

  switch (ret) {
    case 0:
      immediate = false;
      break;
    case 1:
      cqc->makeStatus(true, false, StatusType::CommandSpecificStatus,
                      CommandSpecificStatusCode::Invalid_QueueIdentifier);
      break;
  }

  if (immediate) {
    data.subsystem->complete(this);
  }
}

void DeleteSQ::getStatList(std::vector<Stat> &, std::string) noexcept {}

void DeleteSQ::getStatValues(std::vector<double> &) noexcept {}

void DeleteSQ::resetStatValues() noexcept {}

void DeleteSQ::createCheckpoint(std::ostream &out) noexcept {
  Command::createCheckpoint(out);
}

void DeleteSQ::restoreCheckpoint(std::istream &in) noexcept {
  Command::restoreCheckpoint(in);
}

}  // namespace SimpleSSD::HIL::NVMe
