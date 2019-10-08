// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "hil/nvme/command/get_log_page.hh"

#include "hil/nvme/command/internal.hh"

namespace SimpleSSD::HIL::NVMe {

GetLogPage::GetLogPage(ObjectData &o, Subsystem *s, ControllerData *c)
    : Command(o, s, c), size(0), buffer(nullptr) {
  dmaInitEvent = createEvent([this](uint64_t) { dmaInitDone(); },
                             "HIL::NVMe::GetLogPage::dmaInitEvent");
  dmaCompleteEvent = createEvent([this](uint64_t) { dmaComplete(); },
                                 "HIL::NVMe::GetLogPage::dmaCompleteEvent");
}

GetLogPage::~GetLogPage() {
  if (buffer) {
    free(buffer);
  }

  destroyEvent(dmaInitEvent);
  destroyEvent(dmaCompleteEvent);
}

void GetLogPage::dmaInitDone() {
  // Write buffer to host
  dmaEngine->write(0, size, buffer, dmaCompleteEvent);
}

void GetLogPage::dmaComplete() {
  data.subsystem->complete(this);
}

void GetLogPage::setRequest(SQContext *req) {
  sqc = req;
  auto entry = sqc->getData();

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
  size = (((uint32_t)numdu << 16 | numdl) + 1u) * 4;

  debugprint_command(
      "ADMIN   | Get Log Page | Log %d | Size %d | NSID %u | UUID %u", lid,
      size, nsid, uuid);

  // Make response
  createResponse();

  // Make buffer
  buffer = (uint8_t *)calloc(size, 1);

  switch ((LogPageID)lid) {
    case LogPageID::SMARTInformation: {
      auto health = data.subsystem->getHealth(nsid);

      if (LIKELY(health && offset <= 0x1FC)) {
        size = MIN(size, 0x200 - offset);  // At least 4 bytes

        memcpy(buffer, health->data + offset, size);
      }
      else {
        immediate = true;

        // No such namespace
        cqc->makeStatus(true, false, StatusType::GenericCommandStatus,
                        GenericCommandStatusCode::Invalid_Field);
      }
    } break;
    case LogPageID::ChangedNamespaceList:
      data.controller->getLogPage()->cnl.makeResponse(offset, size, buffer);

      break;
    default:
      immediate = true;

      cqc->makeStatus(true, false, StatusType::CommandSpecificStatus,
                      CommandSpecificStatusCode::Invalid_LogPage);

      break;
  }

  if (immediate) {
    data.subsystem->complete(this);
  }
  else {
    // Data is ready
    createDMAEngine(size, dmaInitEvent);
  }
}

void GetLogPage::getStatList(std::vector<Stat> &, std::string) noexcept {}

void GetLogPage::getStatValues(std::vector<double> &) noexcept {}

void GetLogPage::resetStatValues() noexcept {}

void GetLogPage::createCheckpoint(std::ostream &out) noexcept {
  bool exist;

  Command::createCheckpoint(out);

  BACKUP_SCALAR(out, size);

  exist = buffer != nullptr;
  BACKUP_SCALAR(out, exist);

  if (exist) {
    BACKUP_BLOB(out, buffer, size);
  }
}

void GetLogPage::restoreCheckpoint(std::istream &in) noexcept {
  bool exist;

  Command::restoreCheckpoint(in);

  RESTORE_SCALAR(in, size);

  RESTORE_SCALAR(in, exist);

  if (exist) {
    buffer = (uint8_t *)malloc(size);

    RESTORE_BLOB(in, buffer, size);
  }
}

}  // namespace SimpleSSD::HIL::NVMe
