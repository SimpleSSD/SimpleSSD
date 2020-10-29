// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "hil/nvme/command/internal.hh"

namespace SimpleSSD::HIL::NVMe {

CreateCQ::CreateCQ(ObjectData &o, Subsystem *s) : Command(o, s) {
  eventCreated =
      createEvent([this](uint64_t, uint64_t gcid) { createDone(gcid); },
                  "HIL::NVMe::CreateCQ::eventCreated");
}

void CreateCQ::createDone(uint64_t gcid) {
  auto tag = findTag(gcid);

  subsystem->complete(tag);
}

void CreateCQ::setRequest(ControllerData *cdata, AbstractNamespace *,
                          SQContext *req) {
  auto tag = createTag(cdata, req);
  auto entry = req->getData();

  bool immediate = true;

  // Get parameters
  uint16_t id = entry->dword10 & 0xFFFF;
  uint16_t size = ((entry->dword10 & 0xFFFF0000) >> 16) + 1;
  uint16_t iv = (entry->dword11 & 0xFFFF0000) >> 16;
  bool ien = entry->dword11 & 0x02;
  bool pc = entry->dword11 & 0x01;

  debugprint_command(tag, "ADMIN   | Create I/O Completion Queue");

  // Make response
  tag->createResponse();

  uint32_t maxEntries = (tag->controller->getCapabilities() & 0xFFFF) + 1;

  if (size > maxEntries) {
    tag->cqc->makeStatus(true, false, StatusType::CommandSpecificStatus,
                         CommandSpecificStatusCode::Invalid_QueueSize);
  }
  else {
    auto ret = tag->arbitrator->createIOCQ(entry->dptr1, id, size, iv, ien, pc,
                                           eventCreated, tag->getGCID());

    switch (ret) {
      case 0:
        immediate = false;
        break;
      case 1:
        tag->cqc->makeStatus(
            true, false, StatusType::CommandSpecificStatus,
            CommandSpecificStatusCode::Invalid_QueueIdentifier);

        break;
    }
  }

  if (immediate) {
    subsystem->complete(tag);
  }
}

void CreateCQ::getStatList(std::vector<Stat> &, std::string) noexcept {}

void CreateCQ::getStatValues(std::vector<double> &) noexcept {}

void CreateCQ::resetStatValues() noexcept {}

void CreateCQ::createCheckpoint(std::ostream &out) const noexcept {
  Command::createCheckpoint(out);

  BACKUP_EVENT(out, eventCreated);
}

void CreateCQ::restoreCheckpoint(std::istream &in) noexcept {
  Command::restoreCheckpoint(in);

  RESTORE_EVENT(in, eventCreated);
}

}  // namespace SimpleSSD::HIL::NVMe
