// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "hil/nvme/command/format_nvm.hh"

#include "hil/nvme/command/internal.hh"

namespace SimpleSSD::HIL::NVMe {

FormatNVM::FormatNVM(ObjectData &o, Subsystem *s) : Command(o, s) {
  eventFormatDone =
      createEvent([this](uint64_t, uint64_t gcid) { formatDone(gcid); },
                  "HIL::NVMe::FormatNVM::eventFormatDone");
}

void FormatNVM::formatDone(uint64_t gcid) {
  auto tag = findTag(gcid);

  subsystem->getHIL()->getCommandManager()->destroyCommand(gcid);
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
    auto ret = subsystem->format(nsid, (FormatOption)ses, lbaf, eventFormatDone,
                                 tag->getGCID());

    switch (ret) {
      case 0:
        immediate = false;

        break;
      case 1:
        tag->cqc->makeStatus(false, false, StatusType::GenericCommandStatus,
                             GenericCommandStatusCode::Invalid_Field);

        break;
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

  BACKUP_EVENT(out, eventFormatDone);
}

void FormatNVM::restoreCheckpoint(std::istream &in) noexcept {
  Command::restoreCheckpoint(in);

  RESTORE_EVENT(in, eventFormatDone);
}

}  // namespace SimpleSSD::HIL::NVMe
