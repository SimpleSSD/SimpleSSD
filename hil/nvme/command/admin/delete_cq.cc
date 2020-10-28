// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "hil/nvme/command/internal.hh"

namespace SimpleSSD::HIL::NVMe {

DeleteCQ::DeleteCQ(ObjectData &o, Subsystem *s) : Command(o, s) {}

void DeleteCQ::setRequest(ControllerData *cdata, SQContext *req) {
  auto tag = createTag(cdata, req);
  auto entry = req->getData();

  // Get parameters
  uint16_t id = entry->dword10 & 0xFFFF;

  debugprint_command(tag, "ADMIN   | Delete I/O Completion Queue");

  // Make response
  tag->createResponse();

  auto ret = tag->arbitrator->deleteIOCQ(id);

  switch (ret) {
    case 1:
      tag->cqc->makeStatus(true, false, StatusType::CommandSpecificStatus,
                           CommandSpecificStatusCode::Invalid_QueueIdentifier);
      break;
    case 3:
      tag->cqc->makeStatus(true, false, StatusType::CommandSpecificStatus,
                           CommandSpecificStatusCode::Invalid_QueueDeletion);
      break;
  }

  subsystem->complete(tag);
}

void DeleteCQ::getStatList(std::vector<Stat> &, std::string) noexcept {}

void DeleteCQ::getStatValues(std::vector<double> &) noexcept {}

void DeleteCQ::resetStatValues() noexcept {}

}  // namespace SimpleSSD::HIL::NVMe
