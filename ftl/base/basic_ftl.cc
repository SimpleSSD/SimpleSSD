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

  eventInvalidateSubmit =
      createEvent([this](uint64_t t, uint64_t d) { invalidate_submit(t, d); },
                  "FTL::BasicFTL::eventInvalidateSubmit");

  eventGCTrigger = createEvent([this](uint64_t t, uint64_t) { gc_trigger(t); },
                               "FTL::BasicFTL::eventGCTrigger");

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

void BasicFTL::invalidate(Request *cmd) {
  pMapper->invalidateMapping(cmd, eventInvalidateSubmit);
}

void BasicFTL::invalidate_submit(uint64_t, uint64_t tag) {
  auto req = getRequest(tag);

  warn("Trim and Format not implemented.");

  completeRequest(req);
}

void BasicFTL::gc_trigger(uint64_t) {
  panic("GC not implemented.")
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
