// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "hil/nvme/command/dataset_management.hh"

#include "hil/nvme/command/internal.hh"

namespace SimpleSSD::HIL::NVMe {

DatasetManagement::DatasetManagement(ObjectData &o, Subsystem *s,
                                     ControllerData *c)
    : Command(o, s, c), size(0), buffer(nullptr) {
  dmaInitEvent = createEvent([this](uint64_t) { dmaInitDone(); },
                             "HIL::NVMe::Read::dmaInitEvent");
  dmaCompleteEvent = createEvent([this](uint64_t) { dmaComplete(); },
                                 "HIL::NVMe::Read::dmaCompleteEvent");
  trimDoneEvent = createEvent([this](uint64_t) { trimDone(); },
                              "HIL::NVMe::Read::readDoneEvent");
}

DatasetManagement::~DatasetManagement() {
  if (buffer) {
    free(buffer);
  }

  destroyEvent(dmaInitEvent);
  destroyEvent(dmaCompleteEvent);
  destroyEvent(trimDoneEvent);
}

void DatasetManagement::dmaInitDone() {
  dmaEngine->read(0, size, buffer, dmaCompleteEvent);
}

void DatasetManagement::dmaComplete() {
  auto nslist = data.subsystem->getNamespaceList();
  auto ns = nslist.find(sqc->getData()->namespaceID);
  auto range = ns->second->getInfo()->namespaceRange;

  uint64_t slpn, nlp;
  uint64_t nr = size >> 4;
  Range *trimRange;

  for (uint64_t i = 0; i < nr; i++) {
    trimRange = (Range *)(buffer + (i << 4));

    ns->second->getConvertFunction()(trimRange->slba, trimRange->nlb, slpn, nlp,
                                     nullptr, nullptr);

    if (UNLIKELY(slpn + nlp > range.second)) {
      cqc->makeStatus(true, false, StatusType::GenericCommandStatus,
                      GenericCommandStatusCode::Invalid_Field);

      data.subsystem->complete(this);

      return;
    }

    slpn += range.first;

    trimList.emplace_back(std::make_pair(slpn, nlp));
  }

  auto pHIL = data.subsystem->getHIL();

  std::visit(
      [this](auto &&hil) {
        auto &first = trimList.front();

        hil->trimPages(first.first, first.second, trimDoneEvent);
      },
      pHIL);
}

void DatasetManagement::trimDone() {
  trimList.pop_front();

  if (trimList.size() > 0) {
    auto pHIL = data.subsystem->getHIL();

    std::visit(
        [this](auto &&hil) {
          auto &first = trimList.front();

          hil->trimPages(first.first, first.second, trimDoneEvent);
        },
        pHIL);
  }
  else {
    auto now = getTick();

    debugprint_command(
        "NVM     | Dataset Management | NSID %u | Deallocate | %" PRIu64
        " - %" PRIu64 " (%" PRIu64 ")",
        sqc->getData()->namespaceID, beginAt, now, now - beginAt);

    data.subsystem->complete(this);
  }
}

void DatasetManagement::setRequest(SQContext *req) {
  sqc = req;
  auto entry = sqc->getData();

  // Get parameters
  uint32_t nsid = entry->namespaceID;
  uint8_t nr = (entry->dword10 & 0xFF) + 1;

  // Make response
  createResponse();

  if (entry->dword11 != 0x04) {
    // Not a deallocate
    cqc->makeStatus(true, false, StatusType::GenericCommandStatus,
                    GenericCommandStatusCode::Invalid_Field);

    data.subsystem->complete(this);

    return;
  }

  debugprint_command("NVM     | Dataset Management | NSID %u | Deallocate",
                     nsid);

  auto nslist = data.subsystem->getNamespaceList();
  auto ns = nslist.find(nsid);

  if (UNLIKELY(ns == nslist.end())) {
    cqc->makeStatus(true, false, StatusType::GenericCommandStatus,
                    GenericCommandStatusCode::Invalid_Field);

    data.subsystem->complete(this);

    return;
  }

  // Make buffer
  size = (uint64_t)nr << 4;
  beginAt = getTick();

  createDMAEngine(size, dmaInitEvent);
}

void DatasetManagement::getStatList(std::vector<Stat> &, std::string) noexcept {
}

void DatasetManagement::getStatValues(std::vector<double> &) noexcept {}

void DatasetManagement::resetStatValues() noexcept {}

void DatasetManagement::createCheckpoint(std::ostream &out) const noexcept {
  bool exist;

  Command::createCheckpoint(out);

  BACKUP_SCALAR(out, size);
  BACKUP_SCALAR(out, beginAt);

  exist = buffer != nullptr;
  BACKUP_SCALAR(out, exist);

  if (exist) {
    BACKUP_BLOB(out, buffer, size);
  }

  uint64_t lsize = trimList.size();
  BACKUP_SCALAR(out, lsize);

  for (auto &iter : trimList) {
    BACKUP_SCALAR(out, iter.first);
    BACKUP_SCALAR(out, iter.second);
  }
}

void DatasetManagement::restoreCheckpoint(std::istream &in) noexcept {
  bool exist;

  Command::restoreCheckpoint(in);

  RESTORE_SCALAR(in, size);
  RESTORE_SCALAR(in, beginAt);

  RESTORE_SCALAR(in, exist);

  if (exist) {
    buffer = (uint8_t *)malloc(size);

    RESTORE_BLOB(in, buffer, size);
  }

  uint64_t lsize;
  RESTORE_SCALAR(in, lsize);

  for (uint64_t i = 0; i < lsize; i++) {
    uint64_t f, l;

    RESTORE_SCALAR(in, f);
    RESTORE_SCALAR(in, l);

    trimList.emplace_back(std::make_pair(f, l));
  }
}

}  // namespace SimpleSSD::HIL::NVMe
