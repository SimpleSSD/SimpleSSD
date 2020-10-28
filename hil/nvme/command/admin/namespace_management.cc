// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "hil/nvme/command/internal.hh"

namespace SimpleSSD::HIL::NVMe {

NamespaceManagement::NamespaceManagement(ObjectData &o, Subsystem *s)
    : Command(o, s) {
  dmaInitEvent =
      createEvent([this](uint64_t, uint64_t gcid) { dmaInitDone(gcid); },
                  "HIL::NVMe::NamespaceManagement::dmaInitEvent");
  dmaCompleteEvent =
      createEvent([this](uint64_t, uint64_t gcid) { dmaComplete(gcid); },
                  "HIL::NVMe::NamespaceManagement::dmaCompleteEvent");
}

void NamespaceManagement::dmaComplete(uint64_t gcid) {
  auto tag = findTag(gcid);
  uint8_t *buffer = tag->buffer.data();

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
  auto ret = subsystem->createNamespace(&info, nsid);

  switch (ret) {
    case 0:
      tag->cqc->getData()->dword0 = nsid;

      break;
    case 1:
      tag->cqc->makeStatus(false, false, StatusType::CommandSpecificStatus,
                           CommandSpecificStatusCode::Invalid_Format);

      break;
    case 2:
      tag->cqc->makeStatus(
          false, false, StatusType::CommandSpecificStatus,
          CommandSpecificStatusCode::NamespaceIdentifierUnavailable);

      break;
    case 3:
      tag->cqc->makeStatus(
          false, false, StatusType::CommandSpecificStatus,
          CommandSpecificStatusCode::NamespaceInsufficientCapacity);

      break;
  }

  subsystem->complete(tag);
}

void NamespaceManagement::dmaInitDone(uint64_t gcid) {
  auto tag = findTag(gcid);

  tag->dmaEngine->read(tag->request.getDMA(), 0, 4096, tag->buffer.data(),
                       NoMemoryAccess, dmaCompleteEvent, gcid);
}

void NamespaceManagement::setRequest(ControllerData *cdata, SQContext *req) {
  auto tag = createTag(cdata, req);
  auto entry = req->getData();

  bool sendAEN = false;

  // Get parameters
  uint32_t nsid = entry->namespaceID;
  uint8_t sel = entry->dword10 & 0x0F;

  debugprint_command(tag, "ADMIN   | Namespace Management | Sel %u | NSID %u",
                     sel, nsid);

  // Make response
  tag->createResponse();

  if (sel == 0) {
    // Make buffer
    tag->buffer.resize(4096);

    // Make DMA engine
    tag->createDMAEngine(4096, dmaInitEvent);

    return;
  }
  else if (sel == 1) {
    auto ret = subsystem->destroyNamespace(nsid);

    if (ret == 4) {
      tag->cqc->makeStatus(false, false, StatusType::GenericCommandStatus,
                           GenericCommandStatusCode::Invalid_Field);
    }
    else {
      sendAEN = true;
    }
  }
  else {
    tag->cqc->makeStatus(false, false, StatusType::GenericCommandStatus,
                         GenericCommandStatusCode::Invalid_Field);
  }

  subsystem->complete(tag);

  // Send deleted event
  if (UNLIKELY(sendAEN)) {
    subsystem->scheduleAEN(AsyncEventType::Notice,
                           (uint8_t)NoticeCode::NamespaceAttributeChanged,
                           LogPageID::None);
  }
}

void NamespaceManagement::getStatList(std::vector<Stat> &,
                                      std::string) noexcept {}

void NamespaceManagement::getStatValues(std::vector<double> &) noexcept {}

void NamespaceManagement::resetStatValues() noexcept {}

void NamespaceManagement::createCheckpoint(std::ostream &out) const noexcept {
  Command::createCheckpoint(out);

  BACKUP_EVENT(out, dmaInitEvent);
  BACKUP_EVENT(out, dmaCompleteEvent);
}

void NamespaceManagement::restoreCheckpoint(std::istream &in) noexcept {
  Command::restoreCheckpoint(in);

  RESTORE_EVENT(in, dmaInitEvent);
  RESTORE_EVENT(in, dmaCompleteEvent);
}

}  // namespace SimpleSSD::HIL::NVMe
