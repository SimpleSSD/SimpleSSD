// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "mem/dram/ideal/ideal.hh"

#include "util/algorithm.hh"

namespace SimpleSSD::Memory::DRAM::Ideal {

IdealDRAM::IdealDRAM(ObjectData &o)
    : AbstractDRAM(o),
      scheduler(
          o, "Memory::IdealDRAM::scheduler",
          [this](Request *r) -> uint64_t { return preSubmit(r); },
          [this](Request *r) -> uint64_t { return preSubmit(r); },
          [this](Request *r) { postDone(r); },
          [this](Request *r) { postDone(r); }, Request::backup,
          Request::restore) {
  // Latency of transfer (MemoryPacketSize)
  auto bytesPerClock = 2.0 * pStructure->width * pStructure->chip / 8.0 /
                       pTiming->tCK;  // bytes / ps
  packetLatency = MemoryPacketSize / bytesPerClock;
}

IdealDRAM::~IdealDRAM() {
  // DO NOTHING
}

uint64_t IdealDRAM::preSubmit(Request *) {
  return packetLatency;
}

void IdealDRAM::postDone(Request *req) {
  scheduleNow(req->eid, req->data);

  delete req;
}

void IdealDRAM::read(uint64_t address, Event eid, uint64_t data) {
  auto req = new Request(address, eid, data);

  // Enqueue request
  req->beginAt = getTick();

  readStat.add(MemoryPacketSize);

  scheduler.read(req);
}

void IdealDRAM::write(uint64_t address, Event eid, uint64_t data) {
  auto req = new Request(address, eid, data);

  // Enqueue request
  req->beginAt = getTick();

  writeStat.add(MemoryPacketSize);

  scheduler.write(req);
}

void IdealDRAM::createCheckpoint(std::ostream &out) const noexcept {
  AbstractDRAM::createCheckpoint(out);

  BACKUP_SCALAR(out, packetLatency);

  scheduler.createCheckpoint(out);
}

void IdealDRAM::restoreCheckpoint(std::istream &in) noexcept {
  AbstractDRAM::restoreCheckpoint(in);

  RESTORE_SCALAR(in, packetLatency);

  scheduler.restoreCheckpoint(in);
}

}  // namespace SimpleSSD::Memory::DRAM::Ideal
