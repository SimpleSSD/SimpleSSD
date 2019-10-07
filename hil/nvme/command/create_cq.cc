// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "hil/nvme/command/create_cq.hh"

#include "hil/nvme/command/internal.hh"

namespace SimpleSSD::HIL::NVMe {

CreateCQ::CreateCQ(ObjectData &o, Subsystem *s, ControllerData *c)
    : Command(o, s, c) {
  eventCreated = createEvent([this](uint64_t) { createDone(); },
                             "HIL::NVMe::CreateCQ::eventCreated");
}

CreateCQ::~CreateCQ() {
  // We must delete event
  destroyEvent(eventCreated);
}

void CreateCQ::createDone() {
  data.subsystem->complete(this);
}

void CreateCQ::setRequest(SQContext *req) {
  sqc = req;
  auto entry = sqc->getData();

  bool immediate = true;

  // Get parameters
  uint16_t id = entry->dword10 & 0xFFFF;
  uint16_t size = ((entry->dword10 & 0xFFFF0000) >> 16) + 1;
  uint16_t iv = (entry->dword11 & 0xFFFF0000) >> 16;
  bool ien = entry->dword11 & 0x02;
  bool pc = entry->dword11 & 0x01;

  debugprint_command("ADMIN   | Create I/O Completion Queue");

  // Make response
  createResponse();

  uint32_t maxEntries = (data.controller->getCapabilities() & 0xFFFF) + 1;

  if (size > maxEntries) {
    cqc->makeStatus(true, false, StatusType::CommandSpecificStatus,
                    CommandSpecificStatusCode::Invalid_QueueSize);
  }
  else {
    auto ret = data.arbitrator->createIOCQ(entry->dptr1, id, size, iv, ien, pc,
                                           eventCreated);

    switch (ret) {
      case 0:
        immediate = false;
        break;
      case 1:
        cqc->makeStatus(true, false, StatusType::CommandSpecificStatus,
                        CommandSpecificStatusCode::Invalid_QueueIdentifier);
        break;
    }
  }

  if (immediate) {
    data.subsystem->complete(this);
  }
}

void CreateCQ::getStatList(std::vector<Stat> &, std::string) noexcept {}

void CreateCQ::getStatValues(std::vector<double> &) noexcept {}

void CreateCQ::resetStatValues() noexcept {}

void CreateCQ::createCheckpoint(std::ostream &) noexcept {}

void CreateCQ::restoreCheckpoint(std::istream &) noexcept {}

}  // namespace SimpleSSD::HIL::NVMe
