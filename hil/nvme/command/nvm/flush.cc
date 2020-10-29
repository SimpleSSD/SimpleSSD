// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "hil/nvme/command/internal.hh"

namespace SimpleSSD::HIL::NVMe {

Flush::Flush(ObjectData &o, Subsystem *s) : Command(o, s) {
  eventCompletion =
      createEvent([this](uint64_t t, uint64_t d) { completion(t, d); },
                  "HIL::NVMe::Flush::eventCompletion");
}

void Flush::completion(uint64_t now, uint64_t gcid) {
  auto tag = findTag(gcid);

  debugprint_command(
      tag,
      "NVM     | Flush | NSID %u | %" PRIu64 " - %" PRIu64 " (%" PRIu64 ")",
      tag->sqc->getData()->namespaceID, tag->beginAt, now, now - tag->beginAt);

  subsystem->complete(tag);
}

void Flush::setRequest(ControllerData *cdata, AbstractNamespace *ns,
                       SQContext *req) {
  auto tag = createTag(cdata, req);
  auto entry = req->getData();

  // Get parameters
  uint32_t nsid = entry->namespaceID;

  debugprint_command(tag, "NVM     | Flush | NSID %u", nsid);

  // Make response
  tag->createResponse();
  tag->beginAt = getTick();

  // Make command
  auto pHIL = subsystem->getHIL();

  LPN offset = static_cast<LPN>(0);
  uint64_t length = 0;

  if (nsid == NSID_ALL) {
    auto last = subsystem->getTotalPages();

    offset = static_cast<LPN>(0);
    length = last;
  }
  else {
    if (UNLIKELY(ns == nullptr ||
                 !ns->validateCommand(cdata->controller->getControllerID(), req,
                                      tag->cqc))) {
      if (UNLIKELY(ns == nullptr)) {
        tag->cqc->makeStatus(true, false, StatusType::GenericCommandStatus,
                             GenericCommandStatusCode::Invalid_Field);
      }

      subsystem->complete(tag);

      return;
    }

    auto range = ns->getInfo()->namespaceRange;

    offset = range.first;
    length = range.second;
  }

  // TODO: Fix this
  warn_if(length > std::numeric_limits<uint32_t>::max(),
          "NSSIZE > 4GB * Pagesize.");

  tag->initRequest(eventCompletion);
  tag->request.setAddress(offset, length, pHIL->getLPNSize());
  tag->request.setHostTag(tag->getGCID());
  tag->beginAt = getTick();

  pHIL->flush(&tag->request);
}

void Flush::getStatList(std::vector<Stat> &, std::string) noexcept {}

void Flush::getStatValues(std::vector<double> &) noexcept {}

void Flush::resetStatValues() noexcept {}

void Flush::createCheckpoint(std::ostream &out) const noexcept {
  Command::createCheckpoint(out);

  BACKUP_EVENT(out, eventCompletion);
}

void Flush::restoreCheckpoint(std::istream &in) noexcept {
  Command::restoreCheckpoint(in);

  RESTORE_EVENT(in, eventCompletion);
}

}  // namespace SimpleSSD::HIL::NVMe
