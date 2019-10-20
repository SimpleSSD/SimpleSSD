// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "ftl/base/basic_ftl.hh"

namespace SimpleSSD::FTL {

BasicFTL::BasicFTL(ObjectData &o, CommandManager *m, FIL::FIL *f)
    : AbstractFTL(o, m, f), gcInProgress(false), formatInProgress(0) {
  // Create events
  eventReadDoFIL = createEvent([this](uint64_t, uint64_t d) { read_doFIL(d); },
                               "FTL::BasicFTL::eventReadDoFIL");
  eventReadFILDone = createEvent([this](uint64_t, uint64_t d) { read_done(d); },
                                 "FTL::BasicFTL::eventReadFILDone");
  eventWriteDoFIL =
      createEvent([this](uint64_t, uint64_t d) { write_doFIL(d); },
                  "FTL::BasicFTL::eventWriteDoFIL");
  eventWriteFILDone =
      createEvent([this](uint64_t, uint64_t d) { write_done(d); },
                  "FTL::BasicFTL::eventWriteFILDone");
  eventInvalidateDoFIL =
      createEvent([this](uint64_t, uint64_t d) { invalidate_doFIL(d); },
                  "FTL::BasicFTL::eventInvalidateDoFIL");
  eventGCTrigger = createEvent([this](uint64_t, uint64_t) { gc_trigger(); },
                               "FTL::BasicFTL::eventGCTrigger");
  eventGCGetBlockList =
      createEvent([this](uint64_t, uint64_t) { gc_blockinfo(); },
                  "FTL::BasicFTL::eventGCGetBlockList");
  eventGCRead = createEvent([this](uint64_t, uint64_t) { gc_read(); },
                            "FTL::BasicFTL::eventGCRead");
  eventGCWriteMapping = createEvent([this](uint64_t, uint64_t) { gc_write(); },
                                    "FTL::BasicFTL::eventGCWriteMapping");
  eventGCWrite = createEvent([this](uint64_t, uint64_t) { gc_writeDoFIL(); },
                             "FTL::BasicFTL::eventGCWrite");
  eventGCErase = createEvent([this](uint64_t, uint64_t) { gc_erase(); },
                             "FTL::BasicFTL::eventGCErase");
  eventGCDone = createEvent([this](uint64_t, uint64_t) { gc_done(); },
                            "FTL::BasicFTL::eventGCDone");
}

BasicFTL::~BasicFTL() {}

void BasicFTL::read_find(Command &cmd) {
  pMapper->readMapping(cmd, eventReadDoFIL);
}

void BasicFTL::read_doFIL(uint64_t tag) {
  // Now we have PPN
  pFIL->submit(tag);
}

void BasicFTL::read_done(uint64_t tag) {
  auto &cmd = commandManager->getCommand(tag);

  scheduleNow(cmd.eid, tag);
}

void BasicFTL::write_find(Command &cmd) {
  pMapper->writeMapping(cmd, eventWriteDoFIL);
}

void BasicFTL::write_doFIL(uint64_t tag) {
  // Now we have PPN
  pFIL->submit(tag);
}

void BasicFTL::write_done(uint64_t tag) {
  auto &cmd = commandManager->getCommand(tag);

  scheduleNow(cmd.eid, tag);

  // Check GC threshold for On-demand GC
  if (pMapper->checkGCThreshold() && formatInProgress == 0) {
    scheduleNow(eventGCTrigger);
  }
}

void BasicFTL::invalidate_find(Command &cmd) {
  pMapper->invalidateMapping(cmd, eventInvalidateDoFIL);
}

void BasicFTL::invalidate_doFIL(uint64_t tag) {
  auto &cmd = commandManager->getCommand(tag);

  if (cmd.opcode == Operation::Trim) {
    // Complete here (not erasing blocks)
    scheduleNow(cmd.eid, tag);
  }
  else if (formatInProgress == 0) {
    // Erase block here
    formatInProgress = 100;  // 100% remain
    fctx.eid = cmd.eid;
    fctx.data = tag;

    // Make list
    pMapper->getBlocks(cmd.offset, cmd.length, gcBlockList,
                       eventGCGetBlockList);
  }
  else {
    // TODO: Handle this case
    scheduleNow(cmd.eid, tag);
  }
}

void BasicFTL::gc_trigger() {
  gcInProgress = true;
  formatInProgress = 100;

  // Get block list from allocator
  pAllocator->getVictimBlocks(gcBlockList, eventGCGetBlockList);
}

void BasicFTL::gc_blockinfo() {
  if (LIKELY(gcBlockList.size() > 0)) {
    PPN block = gcBlockList.front();

    gcBlockList.pop_front();
    gcCopyList.blockID = block;
    nextCopyIndex = 0;

    pMapper->getCopyList(gcCopyList, eventGCRead);
  }
  else {
    scheduleNow(eventGCDone);
  }
}

void BasicFTL::gc_read() {
  // Find valid page
  if (LIKELY(!gcCopyList.isEnd())) {
    // Update completion info
    auto &cmd = commandManager->getCommand(*gcCopyList.iter);

    cmd.eid = eventGCWriteMapping;
    cmd.opcode = Operation::Read;

    pFIL->submit(cmd.tag);

    return;
  }

  // Prepare erase
  pFIL->submit(gcCopyList.eraseTag);
}

void BasicFTL::gc_write() {
  auto &cmd = commandManager->getCommand(*gcCopyList.iter);

  pMapper->writeMapping(cmd, eventGCWrite);
}

void BasicFTL::gc_writeDoFIL() {
  // Update completion info
  auto &cmd = commandManager->getCommand(*gcCopyList.iter);

  cmd.eid = eventGCRead;
  cmd.opcode = Operation::Write;

  pFIL->submit(cmd.tag);

  // Next!
  ++gcCopyList.iter;
}

void BasicFTL::gc_erase() {
  scheduleNow(eventGCGetBlockList);

  pMapper->releaseCopyList(gcCopyList);
}

void BasicFTL::gc_done() {
  formatInProgress = 0;

  if (!gcInProgress) {
    // This was format
    scheduleNow(fctx.eid, fctx.data);
  }

  gcInProgress = false;
}

void BasicFTL::submit(uint64_t tag) {
  auto &cmd = commandManager->getCommand(tag);

  switch (cmd.opcode) {
    case Operation::Read:
      read_find(cmd);

      break;
    case Operation::Write:
      write_find(cmd);

      break;
    case Operation::Trim:
    case Operation::Format:
      invalidate_find(cmd);

      break;
    default:
      panic("Unexpected opcode.");

      break;
  }
}

bool BasicFTL::isGC() {
  return gcInProgress;
}

uint8_t BasicFTL::isFormat() {
  if (gcInProgress) {
    return 0;
  }
  else {
    return formatInProgress;
  }
}

void BasicFTL::getStatList(std::vector<Stat> &, std::string) noexcept {}

void BasicFTL::getStatValues(std::vector<double> &) noexcept {}

void BasicFTL::resetStatValues() noexcept {}

void BasicFTL::createCheckpoint(std::ostream &out) const noexcept {
  BACKUP_SCALAR(out, gcInProgress);
  BACKUP_SCALAR(out, nextCopyIndex);

  BACKUP_SCALAR(out, formatInProgress);
  BACKUP_EVENT(out, fctx.eid);
  BACKUP_SCALAR(out, fctx.data);

  gcCopyList.createCheckpoint(out);

  uint64_t size = gcBlockList.size();
  BACKUP_SCALAR(out, size);

  for (auto &iter : gcBlockList) {
    BACKUP_SCALAR(out, iter);
  }

  BACKUP_EVENT(out, eventReadDoFIL);
  BACKUP_EVENT(out, eventReadFILDone);
  BACKUP_EVENT(out, eventWriteDoFIL);
  BACKUP_EVENT(out, eventWriteFILDone);
  BACKUP_EVENT(out, eventInvalidateDoFIL);
  BACKUP_EVENT(out, eventGCTrigger);
  BACKUP_EVENT(out, eventGCGetBlockList);
  BACKUP_EVENT(out, eventGCRead);
  BACKUP_EVENT(out, eventGCWriteMapping);
  BACKUP_EVENT(out, eventGCWrite);
  BACKUP_EVENT(out, eventGCErase);
  BACKUP_EVENT(out, eventGCDone);
}

void BasicFTL::restoreCheckpoint(std::istream &in) noexcept {
  RESTORE_SCALAR(in, gcInProgress);
  RESTORE_SCALAR(in, nextCopyIndex);

  RESTORE_SCALAR(in, formatInProgress);
  RESTORE_EVENT(in, fctx.eid);
  RESTORE_SCALAR(in, fctx.data);

  gcCopyList.restoreCheckpoint(in);

  uint64_t size;
  RESTORE_SCALAR(in, size);

  for (uint64_t i = 0; i < size; i++) {
    PPN ppn;

    RESTORE_SCALAR(in, ppn);

    gcBlockList.emplace_back(ppn);
  }

  RESTORE_EVENT(in, eventReadDoFIL);
  RESTORE_EVENT(in, eventReadFILDone);
  RESTORE_EVENT(in, eventWriteDoFIL);
  RESTORE_EVENT(in, eventWriteFILDone);
  RESTORE_EVENT(in, eventInvalidateDoFIL);
  RESTORE_EVENT(in, eventGCTrigger);
  RESTORE_EVENT(in, eventGCGetBlockList);
  RESTORE_EVENT(in, eventGCRead);
  RESTORE_EVENT(in, eventGCWriteMapping);
  RESTORE_EVENT(in, eventGCWrite);
  RESTORE_EVENT(in, eventGCErase);
  RESTORE_EVENT(in, eventGCDone);
}

}  // namespace SimpleSSD::FTL
