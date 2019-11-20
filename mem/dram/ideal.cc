// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "mem/dram/ideal.hh"

#include "util/algorithm.hh"

namespace SimpleSSD::Memory::DRAM {

Ideal::Ideal(ObjectData &o)
    : AbstractDRAM(o),
      scheduler(
          o, "Memory::DRAM::scheduler",
          [this](Request *r) -> uint64_t { return preSubmit(r); },
          [this](Request *r) -> uint64_t { return preSubmit(r); },
          [this](Request *r) { postDone(r); },
          [this](Request *r) { postDone(r); }, Request::backup,
          Request::restore) {
  pageSize = pStructure->rowSize * pStructure->chip * pStructure->channel;
  interfaceBandwidth = 2.0 * pStructure->width * pStructure->chip *
                       pStructure->channel / 8.0 / pTiming->tCK;
  totalCapacity = pStructure->chipSize * pStructure->chip * pStructure->rank *
                  pStructure->channel;
}

Ideal::~Ideal() {
  // DO NOTHING
}

uint64_t Ideal::preSubmit(Request *req) {
  // Calculate latency
  uint64_t pageCount = (req->length > 0) ? DIVCEIL(req->length, pageSize) : 0;
  uint64_t latency = (uint64_t)(pageCount * (pageSize / interfaceBandwidth));

  return latency;
}

void Ideal::postDone(Request *req) {
  scheduleNow(req->eid, req->data);

  delete req;
}

void Ideal::read(uint64_t address, uint64_t length, Event eid, uint64_t data) {
  auto req = new Request(address, length, eid, data);

  // Stat Update
  readStat.count++;
  readStat.size += length;

  // Schedule callback
  scheduler.read(req);
}

void Ideal::write(uint64_t address, uint64_t length, Event eid, uint64_t data) {
  auto req = new Request(address, length, eid, data);

  // Stat Update
  writeStat.count++;
  writeStat.size += length;

  // Schedule callback
  scheduler.write(req);
}

void Ideal::createCheckpoint(std::ostream &out) const noexcept {
  AbstractDRAM::createCheckpoint(out);

  BACKUP_SCALAR(out, interfaceBandwidth);
  BACKUP_SCALAR(out, pageSize);

  scheduler.createCheckpoint(out);
}

void Ideal::restoreCheckpoint(std::istream &in) noexcept {
  AbstractDRAM::restoreCheckpoint(in);

  RESTORE_SCALAR(in, interfaceBandwidth);
  RESTORE_SCALAR(in, pageSize);

  scheduler.restoreCheckpoint(in);
}

}  // namespace SimpleSSD::Memory::DRAM
