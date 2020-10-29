// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 *         Junhyeok Jang <jhjang@camelab.org>
 */

#include "hil/nvme/command/internal.hh"

namespace SimpleSSD::HIL::NVMe {

Write::Write(ObjectData &o, Subsystem *s) : Command(o, s) {
  eventDMAInitDone =
      createEvent([this](uint64_t, uint64_t d) { dmaInitDone(d); },
                  "HIL::NVMe::Write::eventDMAInitDone");
  eventCompletion =
      createEvent([this](uint64_t t, uint64_t d) { completion(t, d); },
                  "HIL::NVMe::Write::eventCompletion");

  count = 0;
}

void Write::dmaInitDone(uint64_t gcid) {
  auto tag = findTag(gcid);
  auto pHIL = subsystem->getHIL();

  // Perform write
  pHIL->write(&tag->request);
}

void Write::completion(uint64_t now, uint64_t gcid) {
  auto tag = findTag(gcid);

  // Make CQ status
  tag->makeResponse();

  // Get address
  uint64_t slba;
  uint32_t nlb;

  tag->request.getAddress(slba, nlb);

  debugprint_command(tag,
                     "NVM     | Write | NSID %u | %" PRIx64 "h + %xh | %" PRIu64
                     " - %" PRIu64 " (%" PRIu64 ")",
                     tag->sqc->getData()->namespaceID, slba, nlb, tag->beginAt,
                     now, now - tag->beginAt);

  // Handle disk
  auto nslist = subsystem->getNamespaceList();
  auto ns = nslist.find(tag->sqc->getData()->namespaceID);
  auto disk = ns->second->getDisk();
  uint32_t lbaSize = ns->second->getInfo()->lbaSize;

  if (disk) {
    auto buffer = tag->request.getBuffer();

    disk->write(slba * lbaSize, nlb * lbaSize, buffer);
  }

  subsystem->complete(tag);
}

void Write::setRequest(ControllerData *cdata, AbstractNamespace *ns,
                       SQContext *req) {
  auto tag = createTag(cdata, req);
  auto entry = req->getData();

  // Get parameters
  uint32_t nsid = entry->namespaceID;
  uint64_t slba = ((uint64_t)entry->dword11 << 32) | entry->dword10;
  uint16_t nlb = (entry->dword12 & 0xFFFF) + 1;
  // uint8_t dtype = (entry->dword12 >> 20) & 0x0F;
  // bool fua = entry->dword12 & 0x40000000;
  // bool lr = entry->dword12 & 0x80000000;
  // uint16_t dspec = entry->dword13 >> 16;
  // uint8_t dsm = entry->dword13 & 0xFF;

  panic_if(nlb == 0, "Unexpected request length.");

  debugprint_command(tag, "NVM     | Write | NSID %u | %" PRIx64 "h + %xh",
                     nsid, slba, nlb);

  // Make response
  tag->createResponse();

  // Check namespace
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

  // Prepare request
  uint32_t lbaSize = ns->getInfo()->lbaSize;

  tag->initRequest(eventCompletion);
  tag->request.setAddress(slba, nlb, lbaSize);
  tag->request.setHostTag(tag->getGCID());
  tag->beginAt = getTick();
  tag->createDMAEngine(nlb * lbaSize, eventDMAInitDone);

  // Handle disk
  auto disk = ns->getDisk();

  if (disk) {
    // Allocate buffer
    tag->request.createBuffer();
  }

  count++;
}

void Write::getStatList(std::vector<Stat> &list, std::string prefix) noexcept {
  list.emplace_back(prefix + "count", "Number of write command");
}

void Write::getStatValues(std::vector<double> &values) noexcept {
  values.push_back((double)count);
}

void Write::resetStatValues() noexcept {
  count = 0;
}

void Write::createCheckpoint(std::ostream &out) const noexcept {
  Command::createCheckpoint(out);

  BACKUP_EVENT(out, eventDMAInitDone);
  BACKUP_EVENT(out, eventCompletion);
}

void Write::restoreCheckpoint(std::istream &in) noexcept {
  Command::restoreCheckpoint(in);

  RESTORE_EVENT(in, eventDMAInitDone);
  RESTORE_EVENT(in, eventCompletion);
}

}  // namespace SimpleSSD::HIL::NVMe
