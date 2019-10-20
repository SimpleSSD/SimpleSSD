// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "hil/nvme/command/flush.hh"

#include "hil/nvme/command/internal.hh"

namespace SimpleSSD::HIL::NVMe {

Flush::Flush(ObjectData &o, Subsystem *s) : Command(o, s) {
  flushDoneEvent =
      createEvent([this](uint64_t, uint64_t gcid) { flushDone(gcid); },
                  "HIL::NVMe::Flush::readDoneEvent");
}

void Flush::flushDone(uint64_t gcid) {
  auto tag = findDMATag(gcid);
  auto now = getTick();

  debugprint_command(
      tag,
      "NVM     | Flush | NSID %u | %" PRIu64 " - %" PRIu64 " (%" PRIu64 ")",
      tag->sqc->getData()->namespaceID, tag->beginAt, now, now - tag->beginAt);

  subsystem->getHIL()->getCommandManager()->destroyCommand(gcid);
  subsystem->complete(tag);
}

void Flush::setRequest(ControllerData *cdata, SQContext *req) {
  auto tag = createDMATag(cdata, req);
  auto entry = req->getData();

  // Get parameters
  uint32_t nsid = entry->namespaceID;

  debugprint_command(tag, "NVM     | Flush | NSID %u", nsid);

  // Make response
  tag->createResponse();
  tag->beginAt = getTick();

  // Make command
  auto gcid = tag->getGCID();
  auto pHIL = subsystem->getHIL();
  auto mgr = pHIL->getCommandManager();

  LPN offset = 0;
  LPN length = 0;

  if (nsid == NSID_ALL) {
    auto last = subsystem->getTotalPages();

    offset = 0;
    length = last;
  }
  else {
    auto nslist = subsystem->getNamespaceList();
    auto ns = nslist.find(nsid);

    if (UNLIKELY(ns == nslist.end())) {
      tag->cqc->makeStatus(true, false, StatusType::GenericCommandStatus,
                           GenericCommandStatusCode::Invalid_Field);

      subsystem->complete(tag);

      return;
    }

    auto range = ns->second->getInfo()->namespaceRange;

    offset = range.first;
    length = range.second;
  }

  mgr->createHILFlush(gcid, flushDoneEvent, offset, length);

  pHIL->submitCommand(gcid);
}

void Flush::getStatList(std::vector<Stat> &, std::string) noexcept {}

void Flush::getStatValues(std::vector<double> &) noexcept {}

void Flush::resetStatValues() noexcept {}

void Flush::createCheckpoint(std::ostream &out) const noexcept {
  Command::createCheckpoint(out);

  BACKUP_EVENT(out, flushDoneEvent);
}

void Flush::restoreCheckpoint(std::istream &in) noexcept {
  Command::restoreCheckpoint(in);

  RESTORE_EVENT(in, flushDoneEvent);
}

}  // namespace SimpleSSD::HIL::NVMe
