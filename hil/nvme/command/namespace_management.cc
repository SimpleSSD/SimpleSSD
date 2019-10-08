// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "hil/nvme/command/namespace_management.hh"

#include "hil/nvme/command/internal.hh"

namespace SimpleSSD::HIL::NVMe {

NamespaceManagement::NamespaceManagement(ObjectData &o, Subsystem *s,
                                         ControllerData *c)
    : Command(o, s, c), buffer(nullptr) {
  dmaInitEvent = createEvent([this](uint64_t) { dmaInitDone(); },
                             "HIL::NVMe::NamespaceManagement::dmaInitEvent");
  dmaCompleteEvent =
      createEvent([this](uint64_t) { dmaComplete(); },
                  "HIL::NVMe::NamespaceManagement::dmaCompleteEvent");
}

NamespaceManagement::~NamespaceManagement() {
  if (buffer) {
    free(buffer);
  }

  destroyEvent(dmaInitEvent);
  destroyEvent(dmaCompleteEvent);
}

void NamespaceManagement::dmaComplete() {
  // Make namespace information
  NamespaceInformation info;

  info.size = *(uint64_t *)(buffer);
  info.capacity = *(uint64_t *)(buffer + 8);
  info.lbaFormatIndex = buffer[26];
  info.dataProtectionSettings = buffer[29];
  info.namespaceSharingCapabilities = buffer[30];
  info.anaGroupIdentifier = *(uint32_t *)(buffer + 92);
  info.nvmSetIdentifier = *(uint16_t *)(buffer + 100);

  // Execute
  uint32_t nsid = NSID_NONE;
  auto ret = data.subsystem->createNamespace(&info, nsid);

  switch (ret) {
    case 0:
      cqc->getData()->dword0 = nsid;

      break;
    case 1:
      cqc->makeStatus(false, false, StatusType::CommandSpecificStatus,
                      CommandSpecificStatusCode::Invalid_Format);

      break;
    case 2:
      cqc->makeStatus(
          false, false, StatusType::CommandSpecificStatus,
          CommandSpecificStatusCode::NamespaceIdentifierUnavailable);

      break;
    case 3:
      cqc->makeStatus(false, false, StatusType::CommandSpecificStatus,
                      CommandSpecificStatusCode::NamespaceInsufficientCapacity);

      break;
  }

  data.subsystem->complete(this);
}

void NamespaceManagement::dmaInitDone() {
  dmaEngine->read(0, size, buffer, dmaCompleteEvent);
}

void NamespaceManagement::setRequest(SQContext *req) {
  sqc = req;
  auto entry = sqc->getData();

  // Get parameters
  uint32_t nsid = entry->namespaceID;
  uint8_t sel = entry->dword10 & 0x0F;

  debugprint_command("ADMIN   | Namespace Management | Sel %u | NSID %u", sel,
                     nsid);

  // Make response
  createResponse();

  if (sel == 0) {
    // Make buffer
    buffer = (uint8_t *)calloc(size, 1);

    // Make DMA engine
    createDMAEngine(size, dmaInitEvent);

    return;
  }
  else if (sel == 1) {
    auto ret = data.subsystem->destroyNamespace(nsid);

    if (ret == 4) {
      cqc->makeStatus(false, false, StatusType::GenericCommandStatus,
                      GenericCommandStatusCode::Invalid_Field);
    }
  }
  else {
    cqc->makeStatus(false, false, StatusType::GenericCommandStatus,
                    GenericCommandStatusCode::Invalid_Field);
  }

  data.subsystem->complete(this);
}

void NamespaceManagement::getStatList(std::vector<Stat> &,
                                      std::string) noexcept {}

void NamespaceManagement::getStatValues(std::vector<double> &) noexcept {}

void NamespaceManagement::resetStatValues() noexcept {}

void NamespaceManagement::createCheckpoint(std::ostream &out) noexcept {
  bool exist;

  Command::createCheckpoint(out);

  exist = buffer != nullptr;
  BACKUP_SCALAR(out, exist);

  if (exist) {
    BACKUP_BLOB(out, buffer, size);
  }
}

void NamespaceManagement::restoreCheckpoint(std::istream &in) noexcept {
  bool exist;

  Command::restoreCheckpoint(in);

  RESTORE_SCALAR(in, exist);

  if (exist) {
    buffer = (uint8_t *)malloc(size);

    RESTORE_BLOB(in, buffer, size);
  }
}

}  // namespace SimpleSSD::HIL::NVMe
