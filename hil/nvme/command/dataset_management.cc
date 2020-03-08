// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "hil/nvme/command/dataset_management.hh"

#include "hil/nvme/command/internal.hh"

namespace SimpleSSD::HIL::NVMe {

DatasetManagement::DatasetManagement(ObjectData &o, Subsystem *s)
    : Command(o, s) {
  dmaInitEvent =
      createEvent([this](uint64_t, uint64_t gcid) { dmaInitDone(gcid); },
                  "HIL::NVMe::Read::dmaInitEvent");
  dmaCompleteEvent =
      createEvent([this](uint64_t, uint64_t gcid) { dmaComplete(gcid); },
                  "HIL::NVMe::Read::dmaCompleteEvent");
  trimDoneEvent =
      createEvent([this](uint64_t t, uint64_t gcid) { trimDone(t, gcid); },
                  "HIL::NVMe::Read::readDoneEvent");
}

void DatasetManagement::dmaInitDone(uint64_t gcid) {
  auto tag = findTag(gcid);

  tag->dmaEngine->read(tag->request.getDMA(), 0, (uint32_t)tag->buffer.size(),
                       tag->buffer.data(), dmaCompleteEvent, gcid);
}

void DatasetManagement::dmaComplete(uint64_t gcid) {
  auto tag = findTag(gcid);

  auto nslist = subsystem->getNamespaceList();
  auto ns = nslist.find(tag->sqc->getData()->namespaceID);
  auto lbaSize = ns->second->getInfo()->lbaSize;

  uint64_t nr = tag->buffer.size() >> 4;
  Range *trimRange;

  for (uint64_t i = 0; i < nr; i++) {
    trimRange = (Range *)(tag->buffer.data() + (i << 4));

    trimList.emplace_back(trimRange->slba * lbaSize, trimRange->nlb * lbaSize);
  }

  auto pHIL = subsystem->getHIL();
  auto &first = trimList.front();

  tag->initRequest(trimDoneEvent);
  tag->request.setAddress(first.first, first.second, 1);

  pHIL->format(&tag->request, FormatOption::None);
}

void DatasetManagement::trimDone(uint64_t now, uint64_t gcid) {
  auto tag = findTag(gcid);
  auto pHIL = subsystem->getHIL();

  trimList.pop_front();

  if (trimList.size() > 0) {
    auto &first = trimList.front();

    tag->request.setAddress(first.first, first.second, 1);

    pHIL->format(&tag->request, FormatOption::None);
  }
  else {
    debugprint_command(
        tag,
        "NVM     | Dataset Management | NSID %u | Deallocate | %" PRIu64
        " - %" PRIu64 " (%" PRIu64 ")",
        tag->sqc->getData()->namespaceID, tag->beginAt, now,
        now - tag->beginAt);

    subsystem->complete(tag);
  }
}

void DatasetManagement::setRequest(ControllerData *cdata, SQContext *req) {
  auto tag = createTag(cdata, req);
  auto entry = req->getData();

  // Get parameters
  uint32_t nsid = entry->namespaceID;
  uint8_t nr = (entry->dword10 & 0xFF) + 1;

  // Make response
  tag->createResponse();

  if (entry->dword11 != 0x04) {
    // Not a deallocate
    tag->cqc->makeStatus(true, false, StatusType::GenericCommandStatus,
                         GenericCommandStatusCode::Invalid_Field);

    subsystem->complete(tag);

    return;
  }

  debugprint_command(tag, "NVM     | Dataset Management | NSID %u | Deallocate",
                     nsid);

  auto nslist = subsystem->getNamespaceList();
  auto ns = nslist.find(nsid);

  if (UNLIKELY(ns == nslist.end())) {
    tag->cqc->makeStatus(true, false, StatusType::GenericCommandStatus,
                         GenericCommandStatusCode::Invalid_Field);

    subsystem->complete(tag);

    return;
  }

  // Make buffer
  tag->buffer.resize((uint64_t)nr << 4);
  tag->request.setHostTag(tag->getGCID());
  tag->beginAt = getTick();

  tag->createDMAEngine((uint32_t)tag->buffer.size(), dmaInitEvent);
}

void DatasetManagement::getStatList(std::vector<Stat> &, std::string) noexcept {
}

void DatasetManagement::getStatValues(std::vector<double> &) noexcept {}

void DatasetManagement::resetStatValues() noexcept {}

void DatasetManagement::createCheckpoint(std::ostream &out) const noexcept {
  Command::createCheckpoint(out);

  BACKUP_EVENT(out, dmaInitEvent);
  BACKUP_EVENT(out, trimDoneEvent);
  BACKUP_EVENT(out, dmaCompleteEvent);
}

void DatasetManagement::restoreCheckpoint(std::istream &in) noexcept {
  Command::restoreCheckpoint(in);

  RESTORE_EVENT(in, dmaInitEvent);
  RESTORE_EVENT(in, trimDoneEvent);
  RESTORE_EVENT(in, dmaCompleteEvent);
}

}  // namespace SimpleSSD::HIL::NVMe
