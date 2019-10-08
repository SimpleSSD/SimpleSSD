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

Read::Read(ObjectData &o, Subsystem *s, ControllerData *c)
    : Command(o, s, c), size(0), buffer(nullptr) {
  dmaInitEvent = createEvent([this](uint64_t) { dmaInitDone(); },
                             "HIL::NVMe::Read::dmaInitEvent");
  dmaCompleteEvent = createEvent([this](uint64_t) { dmaComplete(); },
                                 "HIL::NVMe::Read::dmaCompleteEvent");
  readDoneEvent = createEvent([this](uint64_t) { readDone(); },
                              "HIL::NVMe::Read::readDoneEvent");
}

Read::~Read() {
  if (buffer) {
    free(buffer);
  }

  destroyEvent(dmaInitEvent);
  destroyEvent(dmaCompleteEvent);
  destroyEvent(readDoneEvent);
}

void Read::dmaInitDone() {
  auto pHIL = data.subsystem->getHIL();

  std::visit(
      [this](auto &&hil) {
        hil->readPages(slpn, nlp, buffer + skipFront, readDoneEvent);
      },
      pHIL);
}

void Read::readDone() {
  dmaEngine->write(0, size - skipFront - skipEnd, buffer + skipFront,
                   dmaCompleteEvent);
}

void Read::dmaComplete() {
  auto now = getTick();

  debugprint_command("NVM     | Read | NSID %u | %" PRIx64 "h + %" PRIx64
                     "h | %" PRIu64 " - %" PRIu64 " (%" PRIu64 ")",
                     sqc->getData()->namespaceID, _slba, _nlb, beginAt, now,
                     now - beginAt);

  data.subsystem->complete(this);
}

void Read::setRequest(SQContext *req) {
  sqc = req;
  auto entry = sqc->getData();

  // Get parameters
  uint32_t nsid = entry->namespaceID;
  uint64_t slba = ((uint64_t)entry->dword11 << 32) | entry->dword10;
  uint16_t nlb = (entry->dword12 & 0xFFFF) + 1;
  // bool fua = entry->dword12 & 0x40000000;
  // bool lr = entry->dword12 & 0x80000000;
  // uint8_t dsm = entry->dword13 & 0xFF;

  debugprint_command("NVM     | Read | NSID %u | %" PRIx64 "h + %" PRIx64 "h",
                     nsid, slba, nlb);

  // Make response
  createResponse();

  // Check namespace
  auto nslist = data.subsystem->getNamespaceList();
  auto ns = nslist.find(nsid);

  if (UNLIKELY(ns == nslist.end())) {
    cqc->makeStatus(true, false, StatusType::GenericCommandStatus,
                    GenericCommandStatusCode::Invalid_Field);

    data.subsystem->complete(this);

    return;
  }

  // Convert unit
  ns->second->getConvertFunction()(slba, nlb, slpn, nlp, &skipFront, &skipEnd);

  // Check range
  auto info = ns->second->getInfo();
  auto range = info->namespaceRange;

  if (UNLIKELY(slpn + nlp > range.second)) {
    cqc->makeStatus(true, false, StatusType::GenericCommandStatus,
                    GenericCommandStatusCode::Invalid_Field);

    data.subsystem->complete(this);

    return;
  }

  slpn += info->namespaceRange.first;

  ns->second->read(nlb * info->lbaSize);

  // Make buffer
  size = nlp * info->lpnSize;
  buffer = (uint8_t *)calloc(size, 1);

  _slba = slba;
  _nlb = nlb;
  beginAt = getTick();

  createDMAEngine(size - skipFront - skipEnd, dmaInitEvent);

  // Handle disk image
  auto disk = ns->second->getDisk();

  if (disk) {
    disk->read(slba, nlb, buffer + skipFront);
  }
}

void Read::getStatList(std::vector<Stat> &, std::string) noexcept {}

void Read::getStatValues(std::vector<double> &) noexcept {}

void Read::resetStatValues() noexcept {}

void Read::createCheckpoint(std::ostream &out) const noexcept {
  bool exist;

  Command::createCheckpoint(out);

  BACKUP_SCALAR(out, slpn);
  BACKUP_SCALAR(out, nlp);
  BACKUP_SCALAR(out, skipFront);
  BACKUP_SCALAR(out, skipEnd);
  BACKUP_SCALAR(out, size);
  BACKUP_SCALAR(out, _slba);
  BACKUP_SCALAR(out, _nlb);
  BACKUP_SCALAR(out, beginAt);

  exist = buffer != nullptr;
  BACKUP_SCALAR(out, exist);

  if (exist) {
    BACKUP_BLOB(out, buffer, size);
  }
}

void Read::restoreCheckpoint(std::istream &in) noexcept {
  bool exist;

  Command::restoreCheckpoint(in);

  RESTORE_SCALAR(in, slpn);
  RESTORE_SCALAR(in, nlp);
  RESTORE_SCALAR(in, skipFront);
  RESTORE_SCALAR(in, skipEnd);
  RESTORE_SCALAR(in, size);
  RESTORE_SCALAR(in, _slba);
  RESTORE_SCALAR(in, _nlb);
  RESTORE_SCALAR(in, beginAt);

  RESTORE_SCALAR(in, exist);

  if (exist) {
    buffer = (uint8_t *)malloc(size);

    RESTORE_BLOB(in, buffer, size);
  }
}

}  // namespace SimpleSSD::HIL::NVMe
