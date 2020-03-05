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
    : AbstractFTL(o, p, f, m, a) {
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

  object.memory->write(req->getDRAMAddress, pageSize, InvalidEventID);
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

  object.memory->read(req->getDRAMAddress, pageSize, InvalidEventID);

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
  BACKUP_SCALAR(out, stat);
  BACKUP_EVENT(out, eventReadSubmit);
  BACKUP_EVENT(out, eventReadDone);
  BACKUP_EVENT(out, eventWriteSubmit);
  BACKUP_EVENT(out, eventWriteDone);
  BACKUP_EVENT(out, eventInvalidateSubmit);
  BACKUP_EVENT(out, eventGCTrigger);
}

void BasicFTL::restoreCheckpoint(std::istream &in) noexcept {
  RESTORE_SCALAR(in, stat);
  RESTORE_EVENT(in, eventReadSubmit);
  RESTORE_EVENT(in, eventReadDone);
  RESTORE_EVENT(in, eventWriteSubmit);
  RESTORE_EVENT(in, eventWriteDone);
  RESTORE_EVENT(in, eventInvalidateSubmit);
  RESTORE_EVENT(in, eventGCTrigger);
}

}  // namespace SimpleSSD::FTL
