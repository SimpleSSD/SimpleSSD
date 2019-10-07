// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "hil/nvme/command/delete_cq.hh"

#include "hil/nvme/command/internal.hh"

namespace SimpleSSD::HIL::NVMe {

DeleteCQ::DeleteCQ(ObjectData &o, Subsystem *s, ControllerData *c)
    : Command(o, s, c) {}

DeleteCQ::~DeleteCQ() {}

void DeleteCQ::setRequest(SQContext *req) {
  sqc = req;
  auto entry = sqc->getData();

  // Get parameters
  uint16_t id = entry->dword10 & 0xFFFF;

  debugprint_command("ADMIN   | Delete I/O Completion Queue");

  // Make response
  createResponse();

  auto ret = data.arbitrator->deleteIOCQ(id);

  switch (ret) {
    case 1:
      cqc->makeStatus(true, false, StatusType::CommandSpecificStatus,
                      CommandSpecificStatusCode::Invalid_QueueIdentifier);
      break;
    case 3:
      cqc->makeStatus(true, false, StatusType::CommandSpecificStatus,
                      CommandSpecificStatusCode::Invalid_QueueDeletion);
      break;
  }

  data.subsystem->complete(this);
}

void DeleteCQ::getStatList(std::vector<Stat> &, std::string) noexcept {}

void DeleteCQ::getStatValues(std::vector<double> &) noexcept {}

void DeleteCQ::resetStatValues() noexcept {}

void DeleteCQ::createCheckpoint(std::ostream &out) noexcept {
  Command::createCheckpoint(out);
}

void DeleteCQ::restoreCheckpoint(std::istream &in) noexcept {
  Command::restoreCheckpoint(in);
}
}  // namespace SimpleSSD::HIL::NVMe
