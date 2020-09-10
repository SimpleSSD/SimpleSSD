// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "hil/nvme/command/get_log_page.hh"

#include "hil/nvme/command/internal.hh"

namespace SimpleSSD::HIL::NVMe {

GetLogPage::GetLogPage(ObjectData &o, Subsystem *s) : Command(o, s) {
  dmaInitEvent =
      createEvent([this](uint64_t, uint64_t gcid) { dmaInitDone(gcid); },
                  "HIL::NVMe::GetLogPage::dmaInitEvent");
  dmaCompleteEvent =
      createEvent([this](uint64_t, uint64_t gcid) { dmaComplete(gcid); },
                  "HIL::NVMe::GetLogPage::dmaCompleteEvent");
}

void GetLogPage::dmaInitDone(uint64_t gcid) {
  auto tag = findTag(gcid);

  // Write buffer to host
  tag->dmaEngine->write(tag->request.getDMA(), 0, (uint32_t)tag->buffer.size(),
                        tag->buffer.data(), NoMemoryAccess, dmaCompleteEvent,
                        gcid);
}

void GetLogPage::dmaComplete(uint64_t gcid) {
  auto tag = findTag(gcid);

  subsystem->complete(tag);
}

void GetLogPage::setRequest(ControllerData *cdata, SQContext *req) {
  auto tag = createTag(cdata, req);
  auto entry = req->getData();

  bool immediate = false;

  // Get parameters
  uint32_t nsid = entry->namespaceID;
  uint16_t numdl = entry->dword10 >> 16;
  // bool rae = (entry->dword10 & 0x00008000) != 0;
  // uint8_t lsp = (entry->dword10 >> 8) & 0x0F;
  uint16_t lid = entry->dword10 & 0xFF;
  // uint16_t lsi = entry->dword11 >> 16;
  uint16_t numdu = entry->dword11 & 0xFFFF;
  uint32_t lopl = entry->dword12;
  uint32_t lopu = entry->dword13;
  uint8_t uuid = entry->dword14 & 0x7F;

  uint64_t offset = ((uint64_t)lopu << 32) | lopl;
  uint32_t size = (((uint32_t)numdu << 16 | numdl) + 1u) * 4;

  debugprint_command(
      tag, "ADMIN   | Get Log Page | Log %d | Size %d | NSID %u | UUID %u", lid,
      size, nsid, uuid);

  // Make response
  tag->createResponse();

  // Make buffer
  tag->buffer.resize(size);
  memset(tag->buffer.data(), 0, size);

  switch ((LogPageID)lid) {
    case LogPageID::ErrorInformation:
      // Do Nothing

      break;
    case LogPageID::SMARTInformation: {
      auto health = subsystem->getHealth(nsid);

      if (LIKELY(health && offset <= 0x1FC)) {
        size = MIN(size, 0x200 - (uint32_t)offset);  // At least 4 bytes

        memcpy(tag->buffer.data(), health->data + offset, size);
      }
      else {
        immediate = true;

        // No such namespace
        tag->cqc->makeStatus(true, false, StatusType::GenericCommandStatus,
                             GenericCommandStatusCode::Invalid_Field);
      }
    } break;
    case LogPageID::FirmwareSlotInformation:
      subsystem->getFirmwareInfo(tag->buffer.data(), offset, size);

      break;
    case LogPageID::ChangedNamespaceList:
      tag->controller->getLogPage()->cnl.makeResponse(offset, size,
                                                      tag->buffer.data());

      break;
    case LogPageID::CommandsSupportedAndEffects:
      subsystem->getCommandEffects(tag->buffer.data(), offset, size);

      break;
    default:
      immediate = true;

      tag->cqc->makeStatus(true, false, StatusType::CommandSpecificStatus,
                           CommandSpecificStatusCode::Invalid_LogPage);

      break;
  }

  if (immediate) {
    subsystem->complete(tag);
  }
  else {
    // Data is ready
    tag->createDMAEngine(size, dmaInitEvent);
  }
}

void GetLogPage::getStatList(std::vector<Stat> &, std::string) noexcept {}

void GetLogPage::getStatValues(std::vector<double> &) noexcept {}

void GetLogPage::resetStatValues() noexcept {}

void GetLogPage::createCheckpoint(std::ostream &out) const noexcept {
  Command::createCheckpoint(out);

  BACKUP_EVENT(out, dmaInitEvent);
  BACKUP_EVENT(out, dmaCompleteEvent);
}

void GetLogPage::restoreCheckpoint(std::istream &in) noexcept {
  Command::restoreCheckpoint(in);

  RESTORE_EVENT(in, dmaInitEvent);
  RESTORE_EVENT(in, dmaCompleteEvent);
}

}  // namespace SimpleSSD::HIL::NVMe
