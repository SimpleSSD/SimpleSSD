// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "hil/nvme/command/create_sq.hh"

#include "hil/nvme/command/internal.hh"

namespace SimpleSSD::HIL::NVMe {

CreateSQ::CreateSQ(ObjectData &o, Subsystem *s, ControllerData *c)
    : Command(o, s, c) {
  eventCreated = createEvent([this](uint64_t) { createDone(); },
                             "HIL::NVMe::CreateSQ::eventCreated");
}

CreateSQ::~CreateSQ() {
  // We must delete event
  destroyEvent(eventCreated);
}

void CreateSQ::createDone() {
  data.subsystem->complete(this);
}

void CreateSQ::setRequest(SQContext *req) {
  sqc = req;
  auto entry = sqc->getData();

  bool immediate = false;

  // Get parameters
  uint16_t id = entry->dword10 & 0xFFFF;
  uint16_t size = ((entry->dword10 & 0xFFFF0000) >> 16) + 1;
  uint16_t cqid = (entry->dword11 & 0xFFFF0000) >> 16;
  uint8_t priority = (entry->dword11 & 0x06) >> 1;
  bool pc = entry->dword11 & 0x01;
  uint16_t setid = entry->dword12 & 0xFFFF;

  debugprint_command("ADMIN   | Create I/O Submission Queue");

  // Make response
  createResponse();

  uint32_t maxEntries = (data.controller->getCapabilities() & 0xFFFF) + 1;

  if (size > maxEntries) {
    immediate = true;

    cqc->makeStatus(true, false, StatusType::CommandSpecificStatus,
                    CommandSpecificStatusCode::Invalid_QueueSize);
  }
  else {
    auto ret = data.arbitrator->createIOSQ(entry->dptr1, id, size, cqid,
                                           priority, pc, setid, eventCreated);

    if (ret != 0) {
      immediate = true;

      cqc->makeStatus(true, false, StatusType::CommandSpecificStatus,
                      (CommandSpecificStatusCode)ret);
    }
  }

  if (immediate) {
    data.subsystem->complete(this);
  }
}

void CreateSQ::getStatList(std::vector<Stat> &, std::string) noexcept {}

void CreateSQ::getStatValues(std::vector<double> &) noexcept {}

void CreateSQ::resetStatValues() noexcept {}

void CreateSQ::createCheckpoint(std::ostream &) noexcept {}

void CreateSQ::restoreCheckpoint(std::istream &) noexcept {}

}  // namespace SimpleSSD::HIL::NVMe
