// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "hil/nvme/command/write.hh"

#include "hil/nvme/command/internal.hh"

namespace SimpleSSD::HIL::NVMe {

Write::Write(ObjectData &o, Subsystem *s, ControllerData *c)
    : Command(o, s, c), size(0), buffer(nullptr) {
  dmaInitEvent = createEvent([this](uint64_t) { dmaInitDone(); },
                             "HIL::NVMe::Write::dmaInitEvent");
  dmaCompleteEvent = createEvent([this](uint64_t) { dmaComplete(); },
                                 "HIL::NVMe::Write::dmaCompleteEvent");
  writeDoneEvent = createEvent([this](uint64_t) { writeDone(); },
                               "HIL::NVMe::Write::readDoneEvent");
}

Write::~Write() {
  if (buffer) {
    free(buffer);
  }

  destroyEvent(dmaInitEvent);
  destroyEvent(dmaCompleteEvent);
  destroyEvent(writeDoneEvent);
}

void Write::dmaInitDone() {
  dmaEngine->read(0, size - skipFront - skipEnd, buffer + skipFront,
                  dmaCompleteEvent);
}

void Write::writeDone() {
  auto now = getTick();

  debugprint_command("NVM     | Write | NSID %u | %" PRIx64 "h + %" PRIx64
                     "h | %" PRIu64 " - %" PRIu64 " (%" PRIu64 ")",
                     sqc->getData()->namespaceID, _slba, _nlb, beginAt, now,
                     now - beginAt);

  data.subsystem->complete(this);
}

void Write::dmaComplete() {
  auto pHIL = data.subsystem->getHIL();

  std::visit(
      [this](auto &&hil) {
        hil->writePages(slpn, nlp, buffer + skipFront,
                        std::make_pair(skipFront, skipEnd), writeDoneEvent);
      },
      pHIL);
}

void Write::setRequest(SQContext *req) {
  sqc = req;
  auto entry = sqc->getData();

  // Get parameters
  uint32_t nsid = entry->namespaceID;
  uint64_t slba = ((uint64_t)entry->dword11 << 32) | entry->dword10;
  uint16_t nlb = (entry->dword12 & 0xFFFF) + 1;
  // uint8_t dtype = (entry->dword12 >> 20) & 0x0F;
  // bool fua = entry->dword12 & 0x40000000;
  // bool lr = entry->dword12 & 0x80000000;
  // uint16_t dspec = entry->dword13 >> 16;
  // uint8_t dsm = entry->dword13 & 0xFF;

  debugprint_command("NVM     | Write | NSID %u | %" PRIx64 "h + %" PRIx64 "h",
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

  ns->second->write(nlb * info->lbaSize);

  // Make buffer
  size = nlp * info->lpnSize;
  buffer = (uint8_t *)calloc(size, 1);

  _slba = slba;
  _nlb = nlb;
  beginAt = getTick();

  createDMAEngine(size - skipFront - skipEnd, dmaInitEvent);
}

void Write::getStatList(std::vector<Stat> &, std::string) noexcept {}

void Write::getStatValues(std::vector<double> &) noexcept {}

void Write::resetStatValues() noexcept {}

void Write::createCheckpoint(std::ostream &out) const noexcept {
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

void Write::restoreCheckpoint(std::istream &in) noexcept {
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
