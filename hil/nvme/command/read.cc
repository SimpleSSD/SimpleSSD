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
  auto pHIL = subsystem->getHIL();

  // Perform read
  pHIL->submitCommand(gcid);
}

void Read::readDone(uint64_t gcid) {
  auto tag = findDMATag(gcid);
  auto pHIL = subsystem->getHIL();
  auto &cmd = pHIL->getCommandManager()->getCommand(gcid);

  uint64_t offset = 0;
  uint32_t size = 0;
  uint32_t skipFront = cmd.subCommandList.front().skipFront;
  uint32_t skipEnd = cmd.subCommandList.back().skipEnd;
  uint32_t completed = 0;

  // Find completed subcommand
  for (auto &iter : cmd.subCommandList) {
    if (iter.status == Status::Done) {
      completed++;

      offset = (iter.lpn - cmd.offset) * iter.buffer.size() - skipFront;
      size = iter.buffer.size();

      if (iter.lpn == cmd.offset) {
        offset = 0;
        size = iter.buffer.size() - skipFront;
      }
      else if (iter.lpn + 1 == cmd.offset + cmd.length) {
        size = iter.buffer.size() - skipEnd;
      }

      // Start DMA for current subcommand
      tag->dmaEngine->write(tag->dmaTag, offset, size,
                            iter.buffer.data() + iter.skipFront,
                            dmaCompleteEvent, gcid);
    }
  }

  if (completed == cmd.subCommandList.size()) {
    cmd.status = Status::Done;
  }
}

void Read::dmaComplete(uint64_t gcid) {
  auto tag = findDMATag(gcid);
  auto pHIL = subsystem->getHIL();
  auto mgr = pHIL->getCommandManager();
  auto &cmd = mgr->getCommand(gcid);

  if (cmd.status == Status::Done) {
    // Done
    auto now = getTick();

    debugprint_command(tag,
                       "NVM     | Read | NSID %u | %" PRIx64 "h + %" PRIx64
                       "h | %" PRIu64 " - %" PRIu64 " (%" PRIu64 ")",
                       tag->sqc->getData()->namespaceID, tag->_slba, tag->_nlb,
                       tag->beginAt, now, now - tag->beginAt);

    mgr->destroyCommand(gcid);
    subsystem->complete(tag);
  }
}

void Read::setRequest(ControllerData *cdata, SQContext *req) {
  auto tag = createDMATag(cdata, req);
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

  ns->second->read(nlb * info->lbaSize);

  // Make command
  auto disk = ns->second->getDisk();
  auto pHIL = subsystem->getHIL();
  auto mgr = pHIL->getCommandManager();
  auto gcid = tag->getGCID();

  mgr->createHILRead(gcid, readDoneEvent, slpn, nlp, skipFront, skipEnd,
                     info->lpnSize);

  if (disk) {
    auto &cmd = mgr->getCommand(gcid);

    for (auto &iter : cmd.subCommandList) {
      disk->read(iter.lpn * info->lpnSize, info->lpnSize, iter.buffer.data());
    }
  }

  tag->_slba = slba;
  tag->_nlb = nlb;
  tag->beginAt = getTick();

  tag->createDMAEngine(nlp * info->lpnSize - skipFront - skipEnd, dmaInitEvent);
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
