// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "hil/nvme/command/dataset_management.hh"

#include "hil/nvme/command/internal.hh"
#include "util/disk.hh"

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
      createEvent([this](uint64_t, uint64_t gcid) { trimDone(gcid); },
                  "HIL::NVMe::Read::readDoneEvent");
}

void DatasetManagement::dmaInitDone(uint64_t gcid) {
  auto tag = findBufferTag(gcid);

  tag->dmaEngine->read(tag->dmaTag, 0, (uint32_t)tag->buffer.size(),
                       tag->buffer.data(), dmaCompleteEvent, gcid);
}

void DatasetManagement::dmaComplete(uint64_t gcid) {
  auto tag = findBufferTag(gcid);

  auto nslist = subsystem->getNamespaceList();
  auto ns = nslist.find(tag->sqc->getData()->namespaceID);
  auto range = ns->second->getInfo()->namespaceRange;
  auto lbaSize = ns->second->getInfo()->lbaSize;

  auto disk = ns->second->getDisk();

  uint64_t slpn;
  uint32_t nlp;
  uint64_t nr = tag->buffer.size() >> 4;
  Range *trimRange;

  for (uint64_t i = 0; i < nr; i++) {
    trimRange = (Range *)(tag->buffer.data() + (i << 4));

    ns->second->getConvertFunction()(trimRange->slba, trimRange->nlb, slpn, nlp,
                                     nullptr, nullptr);

    if (UNLIKELY(slpn + nlp > range.second)) {
      tag->cqc->makeStatus(true, false, StatusType::GenericCommandStatus,
                           GenericCommandStatusCode::Invalid_Field);

      subsystem->complete(tag);

      return;
    }

    slpn += range.first;

    trimList.emplace_back(slpn, nlp);

    if (disk) {
      disk->erase(trimRange->slba * lbaSize, trimRange->nlb * lbaSize);
    }
  }

  auto pHIL = subsystem->getHIL();
  auto mgr = pHIL->getCommandManager();
  auto &first = trimList.front();

  mgr->createHILTrim(gcid, trimDoneEvent, first.first, first.second);

  pHIL->submitCommand(gcid);
}

void DatasetManagement::trimDone(uint64_t gcid) {
  auto tag = findBufferTag(gcid);
  auto pHIL = subsystem->getHIL();
  auto mgr = pHIL->getCommandManager();

  trimList.pop_front();

  if (trimList.size() > 0) {
    auto &first = trimList.front();
    auto &cmd = mgr->getCommand(gcid);

    cmd.offset = first.first;
    cmd.length = first.second;

    pHIL->submitCommand(gcid);
  }
  else {
    auto now = getTick();

    debugprint_command(
        tag,
        "NVM     | Dataset Management | NSID %u | Deallocate | %" PRIu64
        " - %" PRIu64 " (%" PRIu64 ")",
        tag->sqc->getData()->namespaceID, tag->beginAt, now,
        now - tag->beginAt);

    mgr->destroyCommand(gcid);
    subsystem->complete(tag);
  }
}

void DatasetManagement::setRequest(ControllerData *cdata, SQContext *req) {
  auto tag = createBufferTag(cdata, req);
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
