// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "mem/sram/sram.hh"

namespace SimpleSSD::Memory::SRAM {

SRAM::SRAM(ObjectData &o)
    : AbstractSRAM(o),
      scheduler(
          o, "Memory::SRAM::scheduler",
          [this](Request *r) -> uint64_t { return preSubmitRead(r); },
          [this](Request *r) -> uint64_t { return preSubmitWrite(r); },
          [this](Request *r) { postDone(r); },
          [this](Request *r) { postDone(r); }, Request::backup,
          Request::restore),
      lastResetAt(0) {
  // Convert cycle to ps
  pStructure->readCycles = (uint64_t)(pStructure->readCycles * 1000000000000.f /
                                      pStructure->clockSpeed);
  pStructure->writeCycles = (uint64_t)(
      pStructure->writeCycles * 1000000000000.f / pStructure->clockSpeed);

  // Energy/Power calculation shortcut
  busyPower = (double)pStructure->pIDD * pStructure->pVCC;  // mW
  idlePower = (double)pStructure->pISB1 * pStructure->pVCC;
}

SRAM::~SRAM() {}

uint64_t SRAM::preSubmitRead(Request *) {
  return pStructure->readCycles;
}

uint64_t SRAM::preSubmitWrite(Request *) {
  return pStructure->writeCycles;
}

void SRAM::postDone(Request *req) {
  busy.busyEnd(getTick());

  // Call handler
  scheduleNow(req->eid, req->data);

  delete req;
}

void SRAM::read(uint64_t address, Event eid, uint64_t data) {
  auto req = new Request(address, eid, data);

  // Enqueue request
  req->beginAt = getTick();

  busy.busyBegin(req->beginAt);
  readStat.add(MemoryPacketSize);

  scheduler.read(req);
}

void SRAM::write(uint64_t address, Event eid, uint64_t data) {
  auto req = new Request(address, eid, data);

  // Enqueue request
  req->beginAt = getTick();

  busy.busyBegin(req->beginAt);
  writeStat.add(MemoryPacketSize);

  scheduler.write(req);
}

void SRAM::getStatValues(std::vector<double> &values) noexcept {
  // Calculate power
  uint64_t window = getTick() - lastResetAt;
  uint64_t busyTick = busy.getBusyTick(getTick());

  panic_if(window < busyTick, "Invalid tick calculation.");

  totalEnergy = busyTick * busyPower + (window - busyTick) * idlePower;
  averagePower = totalEnergy / window;  // mW

  totalEnergy /= 1000.0;  // to pJ

  AbstractSRAM::getStatValues(values);
}

void SRAM::resetStatValues() noexcept {
  lastResetAt = getTick();

  AbstractSRAM::resetStatValues();
}

void SRAM::createCheckpoint(std::ostream &out) const noexcept {
  AbstractSRAM::createCheckpoint(out);

  BACKUP_SCALAR(out, lastResetAt);
  BACKUP_SCALAR(out, busyPower);
  BACKUP_SCALAR(out, idlePower);

  busy.createCheckpoint(out);

  scheduler.createCheckpoint(out);
}

void SRAM::restoreCheckpoint(std::istream &in) noexcept {
  AbstractSRAM::restoreCheckpoint(in);

  RESTORE_SCALAR(in, lastResetAt);
  RESTORE_SCALAR(in, busyPower);
  RESTORE_SCALAR(in, idlePower);

  busy.restoreCheckpoint(in);

  scheduler.restoreCheckpoint(in);
}

}  // namespace SimpleSSD::Memory::SRAM
