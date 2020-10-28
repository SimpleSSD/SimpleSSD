// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "hil/nvme/command/internal.hh"

namespace SimpleSSD::HIL::NVMe {

FormatNVM::FormatNVM(ObjectData &o, Subsystem *s) : Command(o, s) {
  eventCompletion =
      createEvent([this](uint64_t t, uint64_t d) { completion(t, d); },
                  "HIL::NVMe::FormatNVM::eventCompletion");
}

void FormatNVM::completion(uint64_t now, uint64_t gcid) {
  auto tag = findTag(gcid);

  debugprint_command(
      tag, "ADMIN   | Format NVM | %" PRIu64 " - %" PRIu64 " (%" PRIu64 ")",
      tag->beginAt, now, now - tag->beginAt);

  subsystem->complete(tag);
}

void FormatNVM::setRequest(ControllerData *cdata, SQContext *req) {
  auto tag = createTag(cdata, req);
  auto entry = req->getData();

  bool immediate = true;

  // Get parameters
  uint32_t nsid = entry->namespaceID;
  uint8_t ses = (entry->dword10 & 0x0E00) >> 9;
  bool pil = entry->dword10 & 0x0100;
  uint8_t pi = (entry->dword10 & 0xE0) >> 5;
  bool mset = entry->dword10 & 0x10;
  uint8_t lbaf = entry->dword10 & 0x0F;

  debugprint_command(tag, "ADMIN   | Format NVM | SES %u | NSID %u", ses, nsid);

  // Make response
  tag->createResponse();

  if ((ses != 0x00 && ses != 0x01) || (pil || mset || pi != 0x00)) {
    tag->cqc->makeStatus(false, false, StatusType::GenericCommandStatus,
                         GenericCommandStatusCode::Invalid_Field);
  }
  else if (lbaf >= nLBAFormat) {
    tag->cqc->makeStatus(false, false, StatusType::CommandSpecificStatus,
                         CommandSpecificStatusCode::Invalid_Format);
  }
  else {
    // Update namespace structure
    auto &namespaceList = subsystem->getNamespaceList();
    auto ns = namespaceList.find(nsid);

    if (UNLIKELY(ns == namespaceList.end())) {
      tag->cqc->makeStatus(false, false, StatusType::GenericCommandStatus,
                           GenericCommandStatusCode::Invalid_Field);
    }
    else {
      immediate = false;

      auto info = ns->second->getInfo();
      auto pHIL = subsystem->getHIL();

      info->lbaFormatIndex = lbaf;
      info->lbaSize = lbaSize[lbaf];
      info->size =
          info->namespaceRange.second * pHIL->getLPNSize() / info->lbaSize;
      info->capacity = info->size;
      info->utilization = 0;  // Formatted

      // Do format
      tag->initRequest(eventCompletion);
      tag->request.setAddress(info->namespaceRange.first,
                              info->namespaceRange.second, pHIL->getLPNSize());
      tag->request.setHostTag(tag->getGCID());
      tag->beginAt = getTick();

      pHIL->format(&tag->request, (FormatOption)ses);
    }
  }

  if (immediate) {
    subsystem->complete(tag);
  }
}

void FormatNVM::getStatList(std::vector<Stat> &, std::string) noexcept {}

void FormatNVM::getStatValues(std::vector<double> &) noexcept {}

void FormatNVM::resetStatValues() noexcept {}

void FormatNVM::createCheckpoint(std::ostream &out) const noexcept {
  Command::createCheckpoint(out);

  BACKUP_EVENT(out, eventCompletion);
}

void FormatNVM::restoreCheckpoint(std::istream &in) noexcept {
  Command::restoreCheckpoint(in);

  RESTORE_EVENT(in, eventCompletion);
}

}  // namespace SimpleSSD::HIL::NVMe
