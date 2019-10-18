// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "hil/nvme/command/namespace_attachment.hh"

#include "hil/nvme/command/internal.hh"

namespace SimpleSSD::HIL::NVMe {

NamespaceAttachment::NamespaceAttachment(ObjectData &o, Subsystem *s)
    : Command(o, s) {
  dmaInitEvent =
      createEvent([this](uint64_t, uint64_t gcid) { dmaInitDone(gcid); },
                  "HIL::NVMe::NamespaceAttachment::dmaInitEvent");
  dmaCompleteEvent =
      createEvent([this](uint64_t, uint64_t gcid) { dmaComplete(gcid); },
                  "HIL::NVMe::NamespaceAttachment::dmaCompleteEvent");
}

void NamespaceAttachment::dmaInitDone(uint64_t gcid) {
  auto tag = findBufferTag(gcid);

  tag->dmaEngine->read(tag->dmaTag, 0, 4096, tag->buffer.data(),
                       dmaCompleteEvent, gcid);
}

void NamespaceAttachment::dmaComplete(uint64_t gcid) {
  auto tag = findBufferTag(gcid);
  auto entry = tag->sqc->getData();

  // Get parameters
  uint32_t nsid = entry->namespaceID;
  uint8_t sel = entry->dword10 & 0x0F;

  // Read controller list
  bool bad = false;
  uint8_t ret = 0;

  uint8_t *buffer = tag->buffer.data();
  uint16_t count = *(uint16_t *)buffer;
  std::vector<uint16_t> list;

  list.resize(count);

  for (uint16_t i = 1; i <= count; i++) {
    list.at(i - 1) = *(uint16_t *)(buffer + i * 2);

    if (i > 1 && list.at(i - 2) >= list.at(i - 1)) {
      bad = true;
      break;
    }
  }

  if (UNLIKELY(bad)) {
    tag->cqc->makeStatus(false, false, StatusType::CommandSpecificStatus,
                         CommandSpecificStatusCode::Invalid_ControllerList);
  }
  else {
    // Dry-run to validate all
    for (auto &iter : list) {
      if (sel == 0) {
        // Attach
        ret = subsystem->attachNamespace(iter, nsid);
      }
      else {
        // Detach
        ret = subsystem->detachNamespace(iter, nsid);
      }

      if (ret == 1) {
        tag->cqc->makeStatus(false, false, StatusType::CommandSpecificStatus,
                             CommandSpecificStatusCode::Invalid_ControllerList);

        break;
      }
      else if (ret == 2) {
        tag->cqc->makeStatus(false, false, StatusType::GenericCommandStatus,
                             GenericCommandStatusCode::Invalid_Field);

        break;
      }
      else if (ret == 3) {
        tag->cqc->makeStatus(
            false, false, StatusType::CommandSpecificStatus,
            CommandSpecificStatusCode::NamespaceAlreadyAttached);

        break;
      }
      else if (ret == 4) {
        tag->cqc->makeStatus(false, false, StatusType::CommandSpecificStatus,
                             CommandSpecificStatusCode::NamespaceIsPrivate);

        break;
      }
      else if (ret == 5) {
        tag->cqc->makeStatus(false, false, StatusType::CommandSpecificStatus,
                             CommandSpecificStatusCode::NamespaceNotAttached);

        break;
      }
    }

    // Execute
    if (LIKELY(ret == 0)) {
      for (auto &iter : list) {
        if (sel == 0) {
          subsystem->attachNamespace(iter, nsid, false);
        }
        else {
          subsystem->detachNamespace(iter, nsid, false);
        }
      }
    }
  }

  subsystem->complete(tag);

  if (ret == 0) {
    if (sel == 0) {
      subsystem->scheduleAEN(AsyncEventType::Notice,
                             (uint8_t)NoticeCode::NamespaceAttributeChanged,
                             LogPageID::ChangedNamespaceList);
    }
    else {
      subsystem->scheduleAEN(AsyncEventType::Notice,
                             (uint8_t)NoticeCode::NamespaceAttributeChanged,
                             LogPageID::None);
    }
  }
}

void NamespaceAttachment::setRequest(ControllerData *cdata, SQContext *req) {
  auto tag = createBufferTag(cdata, req);
  auto entry = req->getData();

  // Get parameters
  uint32_t nsid = entry->namespaceID;
  uint8_t sel = entry->dword10 & 0x0F;

  debugprint_command(tag, "ADMIN   | Namespace Attachment | Sel %u | NSID %u",
                     sel, nsid);

  // Make response
  tag->createResponse();

  if (UNLIKELY(sel != 0 && sel != 1)) {
    tag->cqc->makeStatus(false, false, StatusType::GenericCommandStatus,
                         GenericCommandStatusCode::Invalid_Field);

    subsystem->complete(tag);

    return;
  }

  // Make buffer
  tag->buffer.resize(4096);

  // Make DMA engine
  tag->createDMAEngine(4096, dmaInitEvent);
}

void NamespaceAttachment::getStatList(std::vector<Stat> &,
                                      std::string) noexcept {}

void NamespaceAttachment::getStatValues(std::vector<double> &) noexcept {}

void NamespaceAttachment::resetStatValues() noexcept {}

void NamespaceAttachment::createCheckpoint(std::ostream &out) const noexcept {
  Command::createCheckpoint(out);

  BACKUP_EVENT(out, dmaInitEvent);
  BACKUP_EVENT(out, dmaCompleteEvent);
}

void NamespaceAttachment::restoreCheckpoint(std::istream &in) noexcept {
  Command::restoreCheckpoint(in);

  RESTORE_EVENT(in, dmaInitEvent);
  RESTORE_EVENT(in, dmaCompleteEvent);
}

}  // namespace SimpleSSD::HIL::NVMe
