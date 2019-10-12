// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "hil/nvme/command/read.hh"

#include "hil/nvme/command/internal.hh"
#include "util/disk.hh"

namespace SimpleSSD::HIL::NVMe {

Read::Read(ObjectData &o, Subsystem *s) : Command(o, s) {
  dmaInitEvent =
      createEvent([this](uint64_t, uint64_t gcid) { dmaInitDone(gcid); },
                  "HIL::NVMe::Read::dmaInitEvent");
  dmaCompleteEvent =
      createEvent([this](uint64_t, uint64_t gcid) { dmaComplete(gcid); },
                  "HIL::NVMe::Read::dmaCompleteEvent");
  readDoneEvent =
      createEvent([this](uint64_t, uint64_t gcid) { readDone(gcid); },
                  "HIL::NVMe::Read::readDoneEvent");
}

void Read::dmaInitDone(uint64_t gcid) {
  auto tag = findIOTag(gcid);

  auto pHIL = subsystem->getHIL();

  std::visit(
      [this, tag, gcid](auto &&hil) {
        hil->readPages(tag->slpn, tag->nlp, tag->buffer + tag->skipFront,
                       readDoneEvent, gcid);
      },
      pHIL);
}

void Read::readDone(uint64_t gcid) {
  auto tag = findIOTag(gcid);

  tag->dmaEngine->write(tag->dmaTag, 0,
                        tag->size - tag->skipFront - tag->skipEnd,
                        tag->buffer + tag->skipFront, dmaCompleteEvent, gcid);
}

void Read::dmaComplete(uint64_t gcid) {
  auto tag = findIOTag(gcid);

  auto now = getTick();

  debugprint_command(tag,
                     "NVM     | Read | NSID %u | %" PRIx64 "h + %" PRIx64
                     "h | %" PRIu64 " - %" PRIu64 " (%" PRIu64 ")",
                     tag->sqc->getData()->namespaceID, tag->_slba, tag->_nlb,
                     tag->beginAt, now, now - tag->beginAt);

  subsystem->complete(tag);
}

void Read::setRequest(ControllerData *cdata, SQContext *req) {
  auto tag = createIOTag(cdata, req);
  auto entry = req->getData();

  // Get parameters
  uint32_t nsid = entry->namespaceID;
  uint64_t slba = ((uint64_t)entry->dword11 << 32) | entry->dword10;
  uint16_t nlb = (entry->dword12 & 0xFFFF) + 1;
  // bool fua = entry->dword12 & 0x40000000;
  // bool lr = entry->dword12 & 0x80000000;
  // uint8_t dsm = entry->dword13 & 0xFF;

  debugprint_command(tag,
                     "NVM     | Read | NSID %u | %" PRIx64 "h + %" PRIx64 "h",
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

  // Convert unit
  ns->second->getConvertFunction()(slba, nlb, tag->slpn, tag->nlp,
                                   &tag->skipFront, &tag->skipEnd);

  // Check range
  auto info = ns->second->getInfo();
  auto range = info->namespaceRange;

  if (UNLIKELY(tag->slpn + tag->nlp > range.second)) {
    tag->cqc->makeStatus(true, false, StatusType::GenericCommandStatus,
                         GenericCommandStatusCode::Invalid_Field);

    subsystem->complete(tag);

    return;
  }

  tag->slpn += info->namespaceRange.first;

  ns->second->read(nlb * info->lbaSize);

  // Make buffer
  tag->size = tag->nlp * info->lpnSize;
  tag->buffer = (uint8_t *)calloc(tag->size, 1);

  tag->_slba = slba;
  tag->_nlb = nlb;
  tag->beginAt = getTick();

  tag->createDMAEngine(tag->size - tag->skipFront - tag->skipEnd, dmaInitEvent);

  // Handle disk image
  auto disk = ns->second->getDisk();

  if (disk) {
    disk->read(slba, nlb, tag->buffer + tag->skipFront);
  }
}

void Read::completeRequest(CommandTag tag) {
  if (((IOCommandData *)tag)->buffer) {
    free(((IOCommandData *)tag)->buffer);
  }
  if (((IOCommandData *)tag)->dmaTag != InvalidDMATag) {
    tag->dmaEngine->deinit(((IOCommandData *)tag)->dmaTag);
  }

  destroyTag(tag);
}

void Read::getStatList(std::vector<Stat> &, std::string) noexcept {}

void Read::getStatValues(std::vector<double> &) noexcept {}

void Read::resetStatValues() noexcept {}

void Read::createCheckpoint(std::ostream &out) const noexcept {
  Command::createCheckpoint(out);

  BACKUP_EVENT(out, dmaInitEvent);
  BACKUP_EVENT(out, readDoneEvent);
  BACKUP_EVENT(out, dmaCompleteEvent);
}

void Read::restoreCheckpoint(std::istream &in) noexcept {
  Command::restoreCheckpoint(in);

  RESTORE_EVENT(in, dmaInitEvent);
  RESTORE_EVENT(in, readDoneEvent);
  RESTORE_EVENT(in, dmaCompleteEvent);
}

}  // namespace SimpleSSD::HIL::NVMe
