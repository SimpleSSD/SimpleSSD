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
  auto tag = findDMATag(gcid);
  auto pHIL = subsystem->getHIL();
  auto &cmd = pHIL->getCommandManager()->getCommand(gcid);
  auto &scmd = cmd.subCommandList.front();

  uint64_t offset = 0;
  uint32_t size = 0;
  uint32_t skipFront = cmd.subCommandList.front().skipFront;
  uint32_t skipEnd = cmd.subCommandList.back().skipEnd;

  scmd.status = Status::DMA;

  size = scmd.buffer.size();
  offset = (scmd.lpn - cmd.offset) * size - skipFront;

  if (scmd.lpn == cmd.offset) {
    offset = 0;
    size -= skipFront;
  }
  if (scmd.lpn + 1 == cmd.offset + cmd.length) {
    size -= skipEnd;
  }

  // Perform first page DMA
  tag->dmaEngine->read(tag->dmaTag, offset, size,
                       scmd.buffer.data() + scmd.skipFront, dmaCompleteEvent,
                       gcid);
}

void Write::dmaComplete(uint64_t gcid) {
  auto tag = findDMATag(gcid);
  auto pHIL = subsystem->getHIL();
  auto &cmd = pHIL->getCommandManager()->getCommand(gcid);
  auto nslist = subsystem->getNamespaceList();
  auto ns = nslist.find(tag->sqc->getData()->namespaceID);
  auto disk = ns->second->getDisk();

  // Find dma subcommand
  uint32_t i = 0;
  uint32_t scmds = cmd.subCommandList.size();

  for (i = 0; i < scmds; i++) {
    auto &iter = cmd.subCommandList.at(i);

    if (iter.status == Status::DMA) {
      pHIL->submitSubcommand(gcid, i);

      // Handle disk
      if (disk) {
        disk->write(i * iter.buffer.size() + iter.skipFront,
                    iter.buffer.size() - iter.skipFront - iter.skipEnd,
                    iter.buffer.data() + iter.skipFront);
      }

      break;
    }
  }

  // Start next DMA (as we break-ed, i != scmds)
  if (LIKELY(++i < scmds)) {
    auto &scmd = cmd.subCommandList.at(i);

    scmd.status = Status::DMA;

    tag->dmaEngine->read(
        tag->dmaTag,
        i * scmd.buffer.size() - cmd.subCommandList.front().skipFront,
        scmd.buffer.size() - scmd.skipEnd, scmd.buffer.data(), dmaCompleteEvent,
        gcid);
  }
}

void Write::writeDone(uint64_t gcid) {
  auto tag = findDMATag(gcid);
  auto pHIL = subsystem->getHIL();
  auto mgr = pHIL->getCommandManager();
  auto &cmd = mgr->getCommand(gcid);

  cmd.counter++;

  if (cmd.counter == cmd.subCommandList.size()) {
    auto now = getTick();

    debugprint_command(tag,
                       "NVM     | Write | NSID %u | %" PRIx64
                       "h + %xh | %" PRIu64 " - %" PRIu64 " (%" PRIu64 ")",
                       tag->sqc->getData()->namespaceID, tag->_slba, tag->_nlb,
                       tag->beginAt, now, now - tag->beginAt);

    mgr->destroyCommand(gcid);
    subsystem->complete(tag);
  }
}

void Write::setRequest(ControllerData *cdata, SQContext *req) {
  auto tag = createDMATag(cdata, req);
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

  debugprint_command(tag, "NVM     | Write | NSID %u | %" PRIx64 "h + %xh",
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

  ns->second->write(nlb * info->lbaSize);

  // Make command
  auto pHIL = subsystem->getHIL();
  auto mgr = pHIL->getCommandManager();
  auto gcid = tag->getGCID();

  mgr->createHILWrite(gcid, writeDoneEvent, slpn, nlp, skipFront, skipEnd,
                      info->lpnSize);

  tag->_slba = slba;
  tag->_nlb = nlb;
  tag->beginAt = getTick();

  tag->createDMAEngine(nlp * info->lpnSize - skipFront - skipEnd, dmaInitEvent);
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
