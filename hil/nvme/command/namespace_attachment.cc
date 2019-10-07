// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "hil/nvme/command/namespace_attachment.hh"

#include "hil/nvme/command/internal.hh"

namespace SimpleSSD::HIL::NVMe {

NamespaceAttachment::NamespaceAttachment(ObjectData &o, Subsystem *s,
                                         ControllerData *c)
    : Command(o, s, c), buffer(nullptr) {
  dmaInitEvent = createEvent([this](uint64_t) { dmaInitDone(); },
                             "HIL::NVMe::GetLogPage::dmaInitEvent");
  dmaCompleteEvent = createEvent([this](uint64_t) { dmaComplete(); },
                                 "HIL::NVMe::GetLogPage::dmaCompleteEvent");
}

NamespaceAttachment::~NamespaceAttachment() {
  if (buffer) {
    free(buffer);
  }

  destroyEvent(dmaInitEvent);
  destroyEvent(dmaCompleteEvent);
}

void NamespaceAttachment::dmaInitDone() {
  dmaEngine->read(0, size, buffer, dmaCompleteEvent);
}

void NamespaceAttachment::dmaComplete() {
  auto entry = sqc->getData();

  // Get parameters
  uint32_t nsid = entry->namespaceID;
  uint8_t sel = entry->dword10 & 0x0F;

  // Read controller list
  bool bad = false;

  uint16_t count = *(uint16_t *)buffer;
  std::vector<uint16_t> list;

  list.reserve(count);

  for (uint16_t i = 1; i <= count; i++) {
    list.at(i - 1) = *(uint16_t *)(buffer + i * 2);

    if (i > 1 && list.at(i - 2) >= list.at(i - 1)) {
      bad = true;
      break;
    }
  }

  if (UNLIKELY(bad)) {
    cqc->makeStatus(false, false, StatusType::CommandSpecificStatus,
                    CommandSpecificStatusCode::Invalid_ControllerList);
  }
  else {
    uint8_t ret = 0;

    // Dry-run to validate all
    for (auto &iter : list) {
      if (sel == 0) {
        // Attach
        ret = data.subsystem->attachController(iter, nsid);
      }
      else {
        // Detach
        ret = data.subsystem->detachController(iter, nsid);
      }

      if (ret == 1) {
        cqc->makeStatus(false, false, StatusType::CommandSpecificStatus,
                        CommandSpecificStatusCode::Invalid_ControllerList);

        break;
      }
      else if (ret == 2) {
        cqc->makeStatus(false, false, StatusType::GenericCommandStatus,
                        GenericCommandStatusCode::Invalid_Field);

        break;
      }
      else if (ret == 3) {
        cqc->makeStatus(false, false, StatusType::CommandSpecificStatus,
                        CommandSpecificStatusCode::NamespaceAlreadyAttached);

        break;
      }
      else if (ret == 4) {
        cqc->makeStatus(false, false, StatusType::CommandSpecificStatus,
                        CommandSpecificStatusCode::NamespaceIsPrivate);

        break;
      }
      else if (ret == 5) {
        cqc->makeStatus(false, false, StatusType::CommandSpecificStatus,
                        CommandSpecificStatusCode::NamespaceNotAttached);

        break;
      }
    }

    // Execute
    if (LIKELY(ret == 0)) {
      for (auto &iter : list) {
        if (sel == 0) {
          data.subsystem->attachController(iter, nsid, false);
        }
        else {
          data.subsystem->detachController(iter, nsid, false);
        }
      }
    }
  }

  data.subsystem->complete(this);
}

void NamespaceAttachment::setRequest(SQContext *req) {
  sqc = req;
  auto entry = sqc->getData();

  // Get parameters
  uint32_t nsid = entry->namespaceID;
  uint8_t sel = entry->dword10 & 0x0F;

  debugprint_command("ADMIN   | Namespace Attachment | Sel %u | NSID %u", sel,
                     nsid);

  // Make response
  createResponse();

  if (UNLIKELY(sel != 0 && sel != 1)) {
    cqc->makeStatus(false, false, StatusType::GenericCommandStatus,
                    GenericCommandStatusCode::Invalid_Field);

    data.subsystem->complete(this);

    return;
  }

  // Make buffer
  buffer = (uint8_t *)calloc(size, 1);

  // Make DMA engine
  createDMAEngine(size, dmaInitEvent);
}

void NamespaceAttachment::getStatList(std::vector<Stat> &,
                                      std::string) noexcept {}

void NamespaceAttachment::getStatValues(std::vector<double> &) noexcept {}

void NamespaceAttachment::resetStatValues() noexcept {}

void NamespaceAttachment::createCheckpoint(std::ostream &out) noexcept {
  bool exist;

  Command::createCheckpoint(out);

  exist = buffer != nullptr;
  BACKUP_SCALAR(out, exist);

  if (exist) {
    BACKUP_BLOB(out, buffer, size);
  }
}

void NamespaceAttachment::restoreCheckpoint(std::istream &in) noexcept {
  bool exist;

  Command::restoreCheckpoint(in);

  RESTORE_SCALAR(in, exist);

  if (exist) {
    buffer = (uint8_t *)malloc(size);

    RESTORE_BLOB(in, buffer, size);
  }
}

}  // namespace SimpleSSD::HIL::NVMe
