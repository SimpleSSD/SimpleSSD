// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "hil/nvme/command/compare.hh"

#include "hil/nvme/command/internal.hh"
#include "util/disk.hh"

namespace SimpleSSD::HIL::NVMe {

Compare::Compare(ObjectData &o, Subsystem *s) : Command(o, s) {
  dmaInitEvent =
      createEvent([this](uint64_t, uint64_t gcid) { dmaInitDone(gcid); },
                  "HIL::NVMe::Compare::dmaInitEvent");
  dmaCompleteEvent =
      createEvent([this](uint64_t, uint64_t gcid) { dmaComplete(gcid); },
                  "HIL::NVMe::Compare::dmaCompleteEvent");
  readNVMDoneEvent =
      createEvent([this](uint64_t, uint64_t gcid) { readNVMDone(gcid); },
                  "HIL::NVMe::Compare::readDoneEvent");
}

void Compare::dmaInitDone(uint64_t gcid) {
  auto tag = findBufferTag(gcid);
  auto pHIL = subsystem->getHIL();
  auto &cmd = pHIL->getCommandManager()->getCommand(gcid);
  auto &first = cmd.subCommandList.front();
  auto &last = cmd.subCommandList.back();

  // DMA
  tag->dmaEngine->read(
      tag->dmaTag, 0, tag->buffer.size() - first.skipFront - last.skipEnd,
      tag->buffer.data() + first.skipFront, dmaCompleteEvent, gcid);
}

void Compare::dmaComplete(uint64_t gcid) {
  auto tag = findBufferTag(gcid);

  tag->complete |= 0x01;

  if (tag->complete == 0x03) {
    compare(tag);
  }
}

void Compare::readNVMDone(uint64_t gcid) {
  auto tag = findBufferTag(gcid);
  auto pHIL = subsystem->getHIL();
  auto &cmd = pHIL->getCommandManager()->getCommand(gcid);

  uint32_t completed = 0;

  for (auto &iter : cmd.subCommandList) {
    if (iter.status == Status::Done) {
      completed++;
    }
  }

  if (completed == cmd.subCommandList.size()) {
    // We completed all page access
    tag->complete |= 0x02;

    if (tag->complete == 0x03) {
      compare(tag);
    }
  }
}

void Compare::compare(BufferCommandData *tag) {
  auto pHIL = subsystem->getHIL();
  auto &cmd = pHIL->getCommandManager()->getCommand(tag->getGCID());

  auto now = getTick();

  // Compare contents
  bool same = true;
  uint64_t offset = 0;

  for (auto &iter : cmd.subCommandList) {
    same &= memcmp(tag->buffer.data() + offset, iter.buffer.data(),
                   iter.buffer.size()) == 0;

    offset += iter.buffer.size();
  }

  if (!same) {
    tag->cqc->makeStatus(false, false, StatusType::MediaAndDataIntegrityErrors,
                         MediaAndDataIntegrityErrorCode::CompareFailure);
  }

  debugprint_command(tag,
                     "NVM     | Compare | NSID %u | %" PRIx64 "h + %" PRIx64
                     "h | %s | %" PRIu64 " - %" PRIu64 " (%" PRIu64 ")",
                     tag->sqc->getData()->namespaceID, tag->_slba, tag->_nlb,
                     same ? "Success" : "Fail", tag->beginAt, now,
                     now - tag->beginAt);

  subsystem->complete(tag);
}

void Compare::setRequest(ControllerData *cdata, SQContext *req) {
  auto tag = createBufferTag(cdata, req);
  auto entry = req->getData();

  // Get parameters
  uint32_t nsid = entry->namespaceID;
  uint64_t slba = ((uint64_t)entry->dword11 << 32) | entry->dword10;
  uint16_t nlb = (entry->dword12 & 0xFFFF) + 1;
  // bool fua = entry->dword12 & 0x40000000;
  // bool lr = entry->dword12 & 0x80000000;
  // uint8_t dsm = entry->dword13 & 0xFF;

  debugprint_command(
      tag, "NVM     | Compare | NSID %u | %" PRIx64 "h + %" PRIx64 "h", nsid,
      slba, nlb);

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
  LPN slpn;
  uint32_t nlp;
  uint32_t skipFront;
  uint32_t skipEnd;

  ns->second->getConvertFunction()(slba, nlb, slpn, nlp, &skipFront, &skipEnd);

  // Check range
  auto info = ns->second->getInfo();
  auto range = info->namespaceRange;

  if (UNLIKELY(slpn + nlp > range.second)) {
    tag->cqc->makeStatus(true, false, StatusType::GenericCommandStatus,
                         GenericCommandStatusCode::Invalid_Field);

    subsystem->complete(tag);

    return;
  }

  slpn += info->namespaceRange.first;

  // Make buffer
  auto disk = ns->second->getDisk();
  auto pHIL = subsystem->getHIL();
  auto mgr = pHIL->getCommandManager();
  auto gcid = tag->getGCID();

  mgr->createHILRead(gcid, readNVMDoneEvent, slpn, nlp, skipFront, skipEnd,
                     info->lpnSize);

  if (disk) {
    auto &cmd = mgr->getCommand(gcid);

    for (auto &iter : cmd.subCommandList) {
      disk->read(iter.lpn * info->lpnSize, info->lpnSize, iter.buffer.data());
    }
  }

  tag->buffer.resize(info->lpnSize * nlp);
  tag->_slba = slba;
  tag->_nlb = nlb;
  tag->beginAt = getTick();

  tag->createDMAEngine(nlp * info->lpnSize - skipFront - skipEnd, dmaInitEvent);
}

void Compare::getStatList(std::vector<Stat> &, std::string) noexcept {}

void Compare::getStatValues(std::vector<double> &) noexcept {}

void Compare::resetStatValues() noexcept {}

void Compare::createCheckpoint(std::ostream &out) const noexcept {
  Command::createCheckpoint(out);

  BACKUP_EVENT(out, dmaInitEvent);
  BACKUP_EVENT(out, dmaCompleteEvent);
  BACKUP_EVENT(out, readNVMDoneEvent);
}

void Compare::restoreCheckpoint(std::istream &in) noexcept {
  Command::restoreCheckpoint(in);

  RESTORE_EVENT(in, dmaInitEvent);
  RESTORE_EVENT(in, dmaCompleteEvent);
  RESTORE_EVENT(in, readNVMDoneEvent);
}

}  // namespace SimpleSSD::HIL::NVMe
