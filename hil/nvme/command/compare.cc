// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "hil/nvme/command/compare.hh"

#include "hil/nvme/command/internal.hh"

namespace SimpleSSD::HIL::NVMe {

Compare::Compare(ObjectData &o, Subsystem *s, ControllerData *c)
    : Command(o, s, c),
      complete(0),
      size(0),
      bufferHost(nullptr),
      bufferNVM(nullptr) {
  dmaInitEvent = createEvent([this](uint64_t) { dmaInitDone(); },
                             "HIL::NVMe::Compare::dmaInitEvent");
  dmaCompleteEvent = createEvent([this](uint64_t) { dmaComplete(); },
                                 "HIL::NVMe::Compare::dmaCompleteEvent");
  readNVMDoneEvent = createEvent([this](uint64_t) { readNVMDone(); },
                                 "HIL::NVMe::Compare::readDoneEvent");
}

Compare::~Compare() {
  if (bufferHost) {
    free(bufferHost);
  }
  if (bufferNVM) {
    free(bufferNVM);
  }

  destroyEvent(dmaInitEvent);
  destroyEvent(dmaCompleteEvent);
  destroyEvent(readNVMDoneEvent);
}

void Compare::dmaInitDone() {
  dmaEngine->read(0, size - skipFront - skipEnd, bufferHost + skipFront,
                  dmaCompleteEvent);
}

void Compare::dmaComplete() {
  complete |= 0x01;

  if (complete == 0x03) {
    compare();
  }
}

void Compare::readNVMDone() {
  complete |= 0x02;

  if (complete == 0x03) {
    compare();
  }
}

void Compare::compare() {
  auto now = getTick();

  // Compare contents
  bool same = memcmp(bufferHost, bufferNVM, size) == 0;

  if (!same) {
    cqc->makeStatus(false, false, StatusType::MediaAndDataIntegrityErrors,
                    MediaAndDataIntegrityErrorCode::CompareFailure);
  }

  debugprint_command("NVM     | Compare | NSID %u | %" PRIx64 "h + %" PRIx64
                     "h | %s | %" PRIu64 " - %" PRIu64 " (%" PRIu64 ")",
                     sqc->getData()->namespaceID, _slba, _nlb,
                     same ? "Success" : "Fail", beginAt, now, now - beginAt);

  data.subsystem->complete(this);
}

void Compare::setRequest(SQContext *req) {
  sqc = req;
  auto entry = sqc->getData();

  // Get parameters
  uint32_t nsid = entry->namespaceID;
  uint64_t slba = ((uint64_t)entry->dword11 << 32) | entry->dword10;
  uint16_t nlb = (entry->dword12 & 0xFFFF) + 1;
  // bool fua = entry->dword12 & 0x40000000;
  // bool lr = entry->dword12 & 0x80000000;
  // uint8_t dsm = entry->dword13 & 0xFF;

  debugprint_command("NVM     | Compare | NSID %u | %" PRIx64 "h + %" PRIx64
                     "h",
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

  // Make buffer
  size = nlp * info->lpnSize;
  bufferHost = (uint8_t *)calloc(size, 1);
  bufferNVM = (uint8_t *)calloc(size, 1);

  _slba = slba;
  _nlb = nlb;
  beginAt = getTick();

  createDMAEngine(size - skipFront - skipEnd, dmaInitEvent);

  // Read
  auto pHIL = data.subsystem->getHIL();

  std::visit(
      [this](auto &&hil) {
        hil->readPages(slpn, nlp, bufferNVM + skipFront, readNVMDoneEvent);
      },
      pHIL);
}

void Compare::getStatList(std::vector<Stat> &, std::string) noexcept {}

void Compare::getStatValues(std::vector<double> &) noexcept {}

void Compare::resetStatValues() noexcept {}

void Compare::createCheckpoint(std::ostream &out) const noexcept {
  bool exist;

  Command::createCheckpoint(out);

  BACKUP_SCALAR(out, complete);
  BACKUP_SCALAR(out, slpn);
  BACKUP_SCALAR(out, nlp);
  BACKUP_SCALAR(out, skipFront);
  BACKUP_SCALAR(out, skipEnd);
  BACKUP_SCALAR(out, size);
  BACKUP_SCALAR(out, _slba);
  BACKUP_SCALAR(out, _nlb);
  BACKUP_SCALAR(out, beginAt);

  exist = bufferHost != nullptr;
  BACKUP_SCALAR(out, exist);

  if (exist) {
    BACKUP_BLOB(out, bufferHost, size);
  }

  exist = bufferNVM != nullptr;
  BACKUP_SCALAR(out, exist);

  if (exist) {
    BACKUP_BLOB(out, bufferNVM, size);
  }
}

void Compare::restoreCheckpoint(std::istream &in) noexcept {
  bool exist;

  Command::restoreCheckpoint(in);

  RESTORE_SCALAR(in, complete);
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
    bufferHost = (uint8_t *)malloc(size);

    RESTORE_BLOB(in, bufferHost, size);
  }

  RESTORE_SCALAR(in, exist);

  if (exist) {
    bufferNVM = (uint8_t *)malloc(size);

    RESTORE_BLOB(in, bufferNVM, size);
  }
}

}  // namespace SimpleSSD::HIL::NVMe
