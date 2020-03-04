// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "ftl/base/basic_ftl.hh"

namespace SimpleSSD::FTL {

BasicFTL::BasicFTL(ObjectData &o, FTL *p, FIL::FIL *f,
                   Mapping::AbstractMapping *m,
                   BlockAllocator::AbstractAllocator *a)
    : AbstractFTL(o, p, f, m, a), gcInProgress(false), formatInProgress(0) {
  memset(&stat, 0, sizeof(stat));

  pageSize = pMapper->getInfo()->pageSize;

  // Create events
  eventReadSubmit =
      createEvent([this](uint64_t, uint64_t d) { read_submit(d); },
                  "FTL::BasicFTL::eventReadSubmit");
  eventReadDone = createEvent([this](uint64_t, uint64_t d) { read_done(d); },
                              "FTL::BasicFTL::eventReadDone");

  eventWriteSubmit =
      createEvent([this](uint64_t, uint64_t d) { write_submit(d); },
                  "FTL::BasicFTL::eventWriteSubmit");
  eventWriteDone = createEvent([this](uint64_t, uint64_t d) { write_done(d); },
                               "FTL::BasicFTL::eventWriteDone");

  eventInvalidateDoFIL =
      createEvent([this](uint64_t t, uint64_t d) { invalidate_doFIL(t, d); },
                  "FTL::BasicFTL::eventInvalidateDoFIL");
  eventGCTrigger = createEvent([this](uint64_t t, uint64_t) { gc_trigger(t); },
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
  eventGCWriteDone = createEvent([this](uint64_t, uint64_t) { gc_writeDone(); },
                                 "FTL::BasicFTL::eventGCWriteDone");
  eventGCErase = createEvent([this](uint64_t, uint64_t) { gc_erase(); },
                             "FTL::BasicFTL::eventGCErase");
  eventGCEraseDone = createEvent([this](uint64_t, uint64_t) { gc_eraseDone(); },
                                 "FTL::BasicFTL::EventGCEraseDone");
  eventGCDone = createEvent([this](uint64_t t, uint64_t) { gc_done(t); },
                            "FTL::BasicFTL::eventGCDone");

  mergeReadModifyWrite = readConfigBoolean(Section::FlashTranslation,
                                           Config::Key::MergeReadModifyWrite);
}

BasicFTL::~BasicFTL() {}

void BasicFTL::read(Request *cmd) {
  pMapper->readMapping(cmd, eventReadSubmit);
}

void BasicFTL::read_submit(uint64_t tag) {
  auto req = getRequest(tag);

  pFIL->read(FIL::Request(req->getPPN(), eventReadDone, tag));
}

void BasicFTL::read_done(uint64_t tag) {
  auto req = getRequest(tag);

  completeRequest(req);
}

void BasicFTL::write(Request *cmd) {
  LPN slpn = cmd->getLPN();
  uint32_t nlp = 1;

  // Smaller than one logical (super) page
  if (cmd->getLength() != pageSize ||
      pMapper->prepareReadModifyWrite(cmd, slpn, nlp)) {
    // Perform read-modify-write
    panic("RMW not implemented.");

    // TODO: Read slpn + nlp range and write same range
  }
  else {
    pMapper->writeMapping(cmd, eventWriteSubmit);

    return;
  }
}

void BasicFTL::write_submit(uint64_t tag) {
  auto req = getRequest(tag);

  pFIL->program(FIL::Request(req->getPPN(), eventWriteDone, tag));

  triggerGC();
}

void BasicFTL::write_done(uint64_t tag) {
  auto req = getRequest(tag);

  completeRequest(req);
}

void BasicFTL::invalidate(LPN slpn, uint32_t nlp, Event eid, uint64_t data) {
  pMapper->invalidateMapping(cmd, eventInvalidateDoFIL);
}

void BasicFTL::invalidate_doFIL(uint64_t now, uint64_t tag) {
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

    // We already have old PPN in cmd.subCommandList
    // Copy them
    gcBlockList.clear();

    for (auto &scmd : cmd.subCommandList) {
      gcBlockList.emplace_back(scmd.ppn);
    }

    debugprint(Log::DebugID::FTL, "Fmt   | Immediate erase | %u blocks",
               gcBlockList.size());

    gcBeginAt = now;
  }
  else {
    // TODO: Handle this case
    scheduleNow(cmd.eid, tag);
  }
}

void BasicFTL::gc_trigger(uint64_t now) {
  gcInProgress = true;
  formatInProgress = 100;
  gcBeginAt = now;

  stat.count++;

  // Get block list from allocator
  pAllocator->getVictimBlocks(gcBlockList, eventGCGetBlockList);

  debugprint(Log::DebugID::FTL, "GC    | On-demand | %u blocks",
             gcBlockList.size());
}

void BasicFTL::gc_blockinfo() {
  if (LIKELY(gcBlockList.size() > 0)) {
    PPN block = gcBlockList.front();

    gcBlockList.pop_front();
    gcCopyList.blockID = block;

    pMapper->getCopyList(gcCopyList, eventGCRead);
    gcCopyList.resetIterator();
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
    cmd.counter = 0;

    debugprint(Log::DebugID::FTL,
               "GC    | Read  | PPN %" PRIx64 "h + %" PRIx64 "h",
               cmd.subCommandList.front().ppn, cmd.length);

    pFIL->submit(cmd.tag);

    return;
  }

  // Do erase
  auto &cmd = commandManager->getCommand(gcCopyList.eraseTag);

  cmd.eid = eventGCErase;
  cmd.opcode = Operation::Erase;
  cmd.counter = 0;

  debugprint(Log::DebugID::FTL,
             "GC    | Erase | PPN %" PRIx64 "h + %" PRIx64 "h",
             cmd.subCommandList.front().ppn, cmd.length);

  pFIL->submit(gcCopyList.eraseTag);
}

void BasicFTL::gc_write() {
  auto &cmd = commandManager->getCommand(*gcCopyList.iter);

  cmd.counter++;

  if (cmd.counter == cmd.length) {
    PPN old = cmd.subCommandList.front().ppn;

    pMapper->writeMapping(cmd, eventGCWrite);

    debugprint(Log::DebugID::FTL,
               "GC    | Write | PPN %" PRIx64 "h (LPN %" PRIx64
               "h) -> PPN %" PRIx64 "h + %" PRIx64 "h",
               old, cmd.offset, cmd.subCommandList.front().ppn, cmd.length);
  }
}

void BasicFTL::gc_writeDoFIL() {
  // Update completion info
  auto &cmd = commandManager->getCommand(*gcCopyList.iter);

  cmd.eid = eventGCWriteDone;
  cmd.opcode = Operation::Write;
  cmd.counter = 0;

  pFIL->submit(cmd.tag);
}

void BasicFTL::gc_writeDone() {
  auto &cmd = commandManager->getCommand(*gcCopyList.iter);

  cmd.counter++;

  if (cmd.counter == cmd.length) {
    stat.superpages++;
    stat.pages += mappingGranularity;

    // Next!
    ++gcCopyList.iter;

    scheduleNow(eventGCRead);
  }
}

void BasicFTL::gc_erase() {
  auto &cmd = commandManager->getCommand(gcCopyList.eraseTag);

  cmd.counter++;

  if (cmd.counter == cmd.length) {
    pAllocator->reclaimBlocks(gcCopyList.blockID, eventGCEraseDone);
  }
}

void BasicFTL::gc_eraseDone() {
  scheduleNow(eventGCGetBlockList);

  stat.blocks++;

  pMapper->releaseCopyList(gcCopyList);
  gcCopyList.commandList.clear();
}

void BasicFTL::gc_done(uint64_t now) {
  formatInProgress = 0;

  if (gcInProgress) {
    debugprint(Log::DebugID::FTL,
               "GC    | On-demand | %" PRIu64 " - %" PRIu64 " (%" PRIu64 ")",
               gcBeginAt, now, now - gcBeginAt);
  }
  else {
    debugprint(Log::DebugID::FTL,
               "Fmt   | %" PRIu64 " - %" PRIu64 " (%" PRIu64 ")", gcBeginAt,
               now, now - gcBeginAt);

    // This was format
    scheduleNow(fctx.eid, fctx.data);
  }

  gcInProgress = false;

  // Check write pending queue
  for (auto iter = writePendingQueue.begin();
       iter != writePendingQueue.end();) {
    auto pcmd = *iter;

    // Check writable
    if (!pAllocator->stallRequest()) {
      iter = writePendingQueue.erase(iter);

      write_find(*pcmd);
    }
    else {
      ++iter;
    }
  }

  triggerGC();
}

void BasicFTL::getStatList(std::vector<Stat> &list,
                           std::string prefix) noexcept {
  list.emplace_back(prefix + "ftl.gc.count", "Total GC count");
  list.emplace_back(prefix + "ftl.gc.reclaimed_blocks",
                    "Total reclaimed blocks in GC");
  list.emplace_back(prefix + "ftl.gc.superpage_copies",
                    "Total valid superpage copy");
  list.emplace_back(prefix + "ftl.gc.page_copies", "Total valid page copy");
}

void BasicFTL::getStatValues(std::vector<double> &values) noexcept {
  values.push_back((double)stat.count);
  values.push_back((double)stat.blocks);
  values.push_back((double)stat.superpages);
  values.push_back((double)stat.pages);
}

void BasicFTL::resetStatValues() noexcept {
  memset(&stat, 0, sizeof(stat));
}

void BasicFTL::createCheckpoint(std::ostream &out) const noexcept {
  BACKUP_SCALAR(out, mergeReadModifyWrite);
  BACKUP_SCALAR(out, gcInProgress);
  BACKUP_SCALAR(out, gcBeginAt);

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
  BACKUP_EVENT(out, eventReadFull);
  BACKUP_EVENT(out, eventWriteFindDone);
  BACKUP_EVENT(out, eventWriteTranslate);
  BACKUP_EVENT(out, eventWriteDoFIL);
  BACKUP_EVENT(out, eventReadModifyDone);
  BACKUP_EVENT(out, eventWriteDone);
  BACKUP_EVENT(out, eventInvalidateDoFIL);
  BACKUP_EVENT(out, eventGCTrigger);
  BACKUP_EVENT(out, eventGCGetBlockList);
  BACKUP_EVENT(out, eventGCRead);
  BACKUP_EVENT(out, eventGCWriteMapping);
  BACKUP_EVENT(out, eventGCWrite);
  BACKUP_EVENT(out, eventGCWriteDone);
  BACKUP_EVENT(out, eventGCErase);
  BACKUP_EVENT(out, eventGCEraseDone);
  BACKUP_EVENT(out, eventGCDone);
}

void BasicFTL::restoreCheckpoint(std::istream &in) noexcept {
  RESTORE_SCALAR(in, mergeReadModifyWrite);
  RESTORE_SCALAR(in, gcInProgress);
  RESTORE_SCALAR(in, gcBeginAt);

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
  RESTORE_EVENT(in, eventReadFull);
  RESTORE_EVENT(in, eventWriteFindDone);
  RESTORE_EVENT(in, eventWriteTranslate);
  RESTORE_EVENT(in, eventWriteDoFIL);
  RESTORE_EVENT(in, eventReadModifyDone);
  RESTORE_EVENT(in, eventWriteDone);
  RESTORE_EVENT(in, eventInvalidateDoFIL);
  RESTORE_EVENT(in, eventGCTrigger);
  RESTORE_EVENT(in, eventGCGetBlockList);
  RESTORE_EVENT(in, eventGCRead);
  RESTORE_EVENT(in, eventGCWriteMapping);
  RESTORE_EVENT(in, eventGCWrite);
  RESTORE_EVENT(in, eventGCWriteDone);
  RESTORE_EVENT(in, eventGCErase);
  RESTORE_EVENT(in, eventGCEraseDone);
  RESTORE_EVENT(in, eventGCDone);
}

}  // namespace SimpleSSD::FTL
