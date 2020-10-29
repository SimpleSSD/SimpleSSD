// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "hil/nvme/command/internal.hh"

namespace SimpleSSD::HIL::NVMe {

DeleteSQ::DeleteSQ(ObjectData &o, Subsystem *s) : Command(o, s) {
  eventErased =
      createEvent([this](uint64_t, uint64_t gcid) { eraseDone(gcid); },
                  "HIL::NVMe::DeleteSQ::eventErased");
}

void DeleteSQ::eraseDone(uint64_t gcid) {
  auto tag = findTag(gcid);

  subsystem->complete(tag);
}

void DeleteSQ::setRequest(ControllerData *cdata, AbstractNamespace *,
                          SQContext *req) {
  auto tag = createTag(cdata, req);
  auto entry = req->getData();

  bool immediate = true;

  // Get parameters
  uint16_t id = entry->dword10 & 0xFFFF;

  debugprint_command(tag, "ADMIN   | Delete I/O Submission Queue");

  // Make response
  tag->createResponse();

  auto ret = tag->arbitrator->deleteIOSQ(id, eventErased, tag->getGCID());

  switch (ret) {
    case 0:
      immediate = false;
      break;
    case 1:
      tag->cqc->makeStatus(true, false, StatusType::CommandSpecificStatus,
                           CommandSpecificStatusCode::Invalid_QueueIdentifier);
      break;
  }

  if (immediate) {
    subsystem->complete(tag);
  }
}

void DeleteSQ::getStatList(std::vector<Stat> &, std::string) noexcept {}

void DeleteSQ::getStatValues(std::vector<double> &) noexcept {}

void DeleteSQ::resetStatValues() noexcept {}

void DeleteSQ::createCheckpoint(std::ostream &out) const noexcept {
  Command::createCheckpoint(out);

  BACKUP_EVENT(out, eventErased);
}

void DeleteSQ::restoreCheckpoint(std::istream &in) noexcept {
  Command::restoreCheckpoint(in);

  RESTORE_EVENT(in, eventErased);
}

}  // namespace SimpleSSD::HIL::NVMe
