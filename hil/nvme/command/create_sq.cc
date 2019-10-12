// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "hil/nvme/command/create_sq.hh"

#include "hil/nvme/command/internal.hh"

namespace SimpleSSD::HIL::NVMe {

CreateSQ::CreateSQ(ObjectData &o, Subsystem *s) : Command(o, s) {
  eventCreated =
      createEvent([this](uint64_t, uint64_t gcid) { createDone(gcid); },
                  "HIL::NVMe::CreateSQ::eventCreated");
}

CreateSQ::~CreateSQ() {}

void CreateSQ::createDone(uint64_t gcid) {
  auto tag = findTag(gcid);

  subsystem->complete(tag);
}

void CreateSQ::setRequest(ControllerData *cdata, SQContext *req) {
  auto tag = createTag(cdata, req);
  auto entry = req->getData();

  bool immediate = true;

  // Get parameters
  uint16_t id = entry->dword10 & 0xFFFF;
  uint16_t size = ((entry->dword10 & 0xFFFF0000) >> 16) + 1;
  uint16_t cqid = (entry->dword11 & 0xFFFF0000) >> 16;
  uint8_t priority = (entry->dword11 & 0x06) >> 1;
  bool pc = entry->dword11 & 0x01;
  uint16_t setid = entry->dword12 & 0xFFFF;

  debugprint_command(tag, "ADMIN   | Create I/O Submission Queue");

  // Make response
  tag->createResponse();

  uint32_t maxEntries = (tag->controller->getCapabilities() & 0xFFFF) + 1;

  if (size > maxEntries) {
    tag->cqc->makeStatus(true, false, StatusType::CommandSpecificStatus,
                         CommandSpecificStatusCode::Invalid_QueueSize);
  }
  else {
    auto ret = tag->arbitrator->createIOSQ(entry->dptr1, id, size, cqid,
                                           priority, pc, setid, eventCreated);

    switch (ret) {
      case 0:
        immediate = false;
        break;
      case 1:
        tag->cqc->makeStatus(
            true, false, StatusType::CommandSpecificStatus,
            CommandSpecificStatusCode::Invalid_QueueIdentifier);

        break;
      case 2:
        tag->cqc->makeStatus(
            true, false, StatusType::CommandSpecificStatus,
            CommandSpecificStatusCode::Invalid_CompletionQueue);

        break;
    }
  }

  if (immediate) {
    subsystem->complete(tag);
  }
}

void CreateSQ::getStatList(std::vector<Stat> &, std::string) noexcept {}

void CreateSQ::getStatValues(std::vector<double> &) noexcept {}

void CreateSQ::resetStatValues() noexcept {}

void CreateSQ::createCheckpoint(std::ostream &out) const noexcept {
  Command::createCheckpoint(out);

  BACKUP_EVENT(out, eventCreated);
}

void CreateSQ::restoreCheckpoint(std::istream &in) noexcept {
  Command::restoreCheckpoint(in);

  RESTORE_EVENT(in, eventCreated);
}

}  // namespace SimpleSSD::HIL::NVMe
