// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "hil/nvme/command/write.hh"

#include "hil/nvme/command/internal.hh"
#include "util/disk.hh"

namespace SimpleSSD::HIL::NVMe {

Write::Write(ObjectData &o, Subsystem *s) : Command(o, s) {
  dmaInitEvent =
      createEvent([this](uint64_t, uint64_t gcid) { dmaInitDone(gcid); },
                  "HIL::NVMe::Write::dmaInitEvent");
  dmaCompleteEvent =
      createEvent([this](uint64_t, uint64_t gcid) { dmaComplete(gcid); },
                  "HIL::NVMe::Write::dmaCompleteEvent");
  writeDoneEvent =
      createEvent([this](uint64_t, uint64_t gcid) { writeDone(gcid); },
                  "HIL::NVMe::Write::readDoneEvent");
}

void Write::dmaInitDone(uint64_t gcid) {
  auto tag = findIOTag(gcid);

  tag->dmaEngine->read(tag->dmaTag, 0,
                       tag->size - tag->skipFront - tag->skipEnd,
                       tag->buffer + tag->skipFront, dmaCompleteEvent, gcid);
}

void Write::writeDone(uint64_t gcid) {
  auto tag = findIOTag(gcid);

  auto now = getTick();

  debugprint_command(tag,
                     "NVM     | Write | NSID %u | %" PRIx64 "h + %" PRIx64
                     "h | %" PRIu64 " - %" PRIu64 " (%" PRIu64 ")",
                     tag->sqc->getData()->namespaceID, tag->_slba, tag->_nlb,
                     tag->beginAt, now, now - tag->beginAt);

  subsystem->complete(tag);
}

void Write::dmaComplete(uint64_t gcid) {
  auto tag = findIOTag(gcid);

  auto nslist = subsystem->getNamespaceList();
  auto ns = nslist.find(tag->sqc->getData()->namespaceID);

  // Handle disk image
  auto disk = ns->second->getDisk();

  if (disk) {
    disk->write(tag->_slba, tag->_nlb, tag->buffer + tag->skipFront);
  }

  auto pHIL = subsystem->getHIL();

  std::visit(
      [this, tag, gcid](auto &&hil) {
        hil->writePages(tag->slpn, tag->nlp, tag->buffer + tag->skipFront,
                        std::make_pair(tag->skipFront, tag->skipEnd),
                        writeDoneEvent, gcid);
      },
      pHIL);
}

void Write::setRequest(ControllerData *cdata, SQContext *req) {
  auto tag = createIOTag(cdata, req);
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

  debugprint_command(tag,
                     "NVM     | Write | NSID %u | %" PRIx64 "h + %" PRIx64 "h",
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

  ns->second->write(nlb * info->lbaSize);

  // Make buffer
  tag->size = tag->nlp * info->lpnSize;
  tag->buffer = (uint8_t *)calloc(tag->size, 1);

  tag->_slba = slba;
  tag->_nlb = nlb;
  tag->beginAt = getTick();

  tag->createDMAEngine(tag->size - tag->skipFront - tag->skipEnd, dmaInitEvent);
}

void Write::completeRequest(CommandTag tag) {
  if (((IOCommandData *)tag)->buffer) {
    free(((IOCommandData *)tag)->buffer);
  }

  destroyTag(tag);
}

void Write::getStatList(std::vector<Stat> &, std::string) noexcept {}

void Write::getStatValues(std::vector<double> &) noexcept {}

void Write::resetStatValues() noexcept {}

void Write::createCheckpoint(std::ostream &out) const noexcept {
  Command::createCheckpoint(out);

  BACKUP_EVENT(out, dmaInitEvent);
  BACKUP_EVENT(out, writeDoneEvent);
  BACKUP_EVENT(out, dmaCompleteEvent);
}

void Write::restoreCheckpoint(std::istream &in) noexcept {
  Command::restoreCheckpoint(in);

  RESTORE_EVENT(in, dmaInitEvent);
  RESTORE_EVENT(in, writeDoneEvent);
  RESTORE_EVENT(in, dmaCompleteEvent);
}

}  // namespace SimpleSSD::HIL::NVMe
