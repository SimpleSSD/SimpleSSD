// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "hil/nvme/command/compare.hh"

#include "hil/nvme/command/internal.hh"

namespace SimpleSSD::HIL::NVMe {

Compare::Compare(ObjectData &o, Subsystem *s) : Command(o, s) {
  eventDMAInitDone =
      createEvent([this](uint64_t, uint64_t d) { dmaInitDone(d); },
                  "HIL::NVMe::Compare::eventDMAInitDone");
  eventCompletion =
      createEvent([this](uint64_t t, uint64_t d) { completion(t, d); },
                  "HIL::NVMe::Compare::eventCompletion");
}

void Compare::dmaInitDone(uint64_t gcid) {
  auto tag = findTag(gcid);
  auto pHIL = subsystem->getHIL();

  // Submit compare request
  // TODO: Support FUSED operation
  pHIL->compare(&tag->request, false);
}

void Compare::completion(uint64_t now, uint64_t gcid) {
  auto tag = findTag(gcid);

  // Make CQ status
  tag->makeResponse();

  // Get address
  uint64_t slba;
  uint32_t nlb;

  tag->request.getAddress(slba, nlb);

  // For log only (already filled in makeResponse)
  bool diff = tag->request.getResponse() == Response::CompareFail;

  debugprint_command(tag,
                     "NVM     | Compare | NSID %u | %" PRIx64
                     "h + %xh | %s | %" PRIu64 " - %" PRIu64 " (%" PRIu64 ")",
                     tag->sqc->getData()->namespaceID, slba, nlb,
                     diff ? "Fail" : "Success", tag->beginAt, now,
                     now - tag->beginAt);

  subsystem->complete(tag);
}

void Compare::setRequest(ControllerData *cdata, SQContext *req) {
  auto tag = createTag(cdata, req);
  auto entry = req->getData();

  // Get parameters
  uint32_t nsid = entry->namespaceID;
  uint64_t slba = ((uint64_t)entry->dword11 << 32) | entry->dword10;
  uint16_t nlb = (entry->dword12 & 0xFFFF) + 1;
  // bool fua = entry->dword12 & 0x40000000;
  // bool lr = entry->dword12 & 0x80000000;
  // uint8_t dsm = entry->dword13 & 0xFF;

  debugprint_command(tag, "NVM     | Compare | NSID %u | %" PRIx64 "h + %xh",
                     nsid, slba, nlb);

  // Make response
  tag->createResponse();

  // Check namespace
  auto nslist = subsystem->getNamespaceList();
  auto ns = nslist.find(nsid);

  if (UNLIKELY(ns == nslist.end())) {
    tag->cqc->makeStatus(true, false, StatusType::GenericCommandStatus,
                         GenericCommandStatusCode::Invalid_Field);

    subsystem->complete(tag);

    return;
  }

  // Prepare request
  uint32_t lbaSize = ns->second->getInfo()->lbaSize;

  tag->initRequest(eventCompletion);
  tag->request.setAddress(slba, nlb, lbaSize);
  tag->beginAt = getTick();
  tag->createDMAEngine(nlb * lbaSize, eventDMAInitDone);
}

void Compare::getStatList(std::vector<Stat> &, std::string) noexcept {}

void Compare::getStatValues(std::vector<double> &) noexcept {}

void Compare::resetStatValues() noexcept {}

void Compare::createCheckpoint(std::ostream &out) const noexcept {
  Command::createCheckpoint(out);

  BACKUP_EVENT(out, eventDMAInitDone);
  BACKUP_EVENT(out, eventCompletion);
}

void Compare::restoreCheckpoint(std::istream &in) noexcept {
  Command::restoreCheckpoint(in);

  RESTORE_EVENT(in, eventDMAInitDone);
  RESTORE_EVENT(in, eventCompletion);
}

}  // namespace SimpleSSD::HIL::NVMe
