// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "hil/nvme/command/flush.hh"

#include "hil/nvme/command/internal.hh"

namespace SimpleSSD::HIL::NVMe {

Flush::Flush(ObjectData &o, Subsystem *s, ControllerData *c)
    : Command(o, s, c) {
  flushDoneEvent = createEvent([this](uint64_t) { flushDone(); },
                               "HIL::NVMe::Flush::readDoneEvent");
}

Flush::~Flush() {
  destroyEvent(flushDoneEvent);
}

void Flush::flushDone() {
  auto now = getTick();

  debugprint_command("NVM     | FLUSH | NSID %u | %" PRIu64 " - %" PRIu64
                     " (%" PRIu64 ")",
                     sqc->getData()->namespaceID, beginAt, now, now - beginAt);

  data.subsystem->complete(this);
}

void Flush::setRequest(SQContext *req) {
  sqc = req;
  auto entry = sqc->getData();

  // Get parameters
  uint32_t nsid = entry->namespaceID;

  debugprint_command("NVM     | FLUSH | NSID %u", nsid);

  // Make response
  createResponse();

  if (nsid == NSID_ALL) {
    auto last = data.subsystem->getTotalPages();
    auto pHIL = data.subsystem->getHIL();

    std::visit(
        [this, last](auto &&hil) { hil->flushCache(0, last, flushDoneEvent); },
        pHIL);
  }
  else {
    auto nslist = data.subsystem->getNamespaceList();
    auto ns = nslist.find(nsid);

    if (UNLIKELY(ns == nslist.end())) {
      cqc->makeStatus(true, false, StatusType::GenericCommandStatus,
                      GenericCommandStatusCode::Invalid_Field);

      data.subsystem->complete(this);

      return;
    }

    auto range = ns->second->getInfo()->namespaceRange;
    auto pHIL = data.subsystem->getHIL();

    std::visit(
        [this, range](auto &&hil) {
          hil->flushCache(range.first, range.second, flushDoneEvent);
        },
        pHIL);
  }
}

void Flush::getStatList(std::vector<Stat> &, std::string) noexcept {}

void Flush::getStatValues(std::vector<double> &) noexcept {}

void Flush::resetStatValues() noexcept {}

void Flush::createCheckpoint(std::ostream &out) const noexcept {
  Command::createCheckpoint(out);

  BACKUP_SCALAR(out, beginAt);
}

void Flush::restoreCheckpoint(std::istream &in) noexcept {
  Command::restoreCheckpoint(in);

  RESTORE_SCALAR(in, beginAt);
}

}  // namespace SimpleSSD::HIL::NVMe
