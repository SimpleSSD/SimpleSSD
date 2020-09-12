// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "mem/dram/ideal/ideal.hh"

#include "util/algorithm.hh"

namespace SimpleSSD::Memory::DRAM {

IdealDRAM::IdealDRAM(ObjectData &o)
    : AbstractDRAM(o),
      scheduler(
          o, "Memory::IdealDRAM::scheduler",
          [this](Request *r) -> uint64_t { return preSubmit(r); },
          [this](Request *r) { postDone(r); }, Request::backup,
          Request::restore),
      totalEnergy(0.0),
      totalPower(0.0) {
  // Latency of transfer (MemoryPacketSize)
  auto bytesPerClock = 2.0 * pStructure->channel * pStructure->width *
                       pStructure->chip / 8.0 / pTiming->tCK;  // bytes / ps
  packetLatency = MemoryPacketSize / bytesPerClock;

  panic_if(!convertMemspec(object, spec), "Memspec conversion failed.");

  dramPower = new libDRAMPower(spec, false);
}

IdealDRAM::~IdealDRAM() {
  delete dramPower;
}

uint64_t IdealDRAM::preSubmit(Request *req) {
  uint64_t cycle = getTick() / pTiming->tCK;

  if (req->read) {
    dramPower->doCommand(Data::MemCommand::ACT, 0, cycle);

    cycle += spec.memTimingSpec.RCD;
    dramPower->doCommand(Data::MemCommand::RD, 0, cycle);

    cycle += spec.memTimingSpec.RAS;
    dramPower->doCommand(Data::MemCommand::PRE, 0, cycle);
  }
  else {
    dramPower->doCommand(Data::MemCommand::ACT, 0, cycle);

    cycle += spec.memTimingSpec.RCD;
    dramPower->doCommand(Data::MemCommand::WR, 0, cycle);

    cycle += spec.memTimingSpec.RAS;
    dramPower->doCommand(Data::MemCommand::PRE, 0, cycle);
  }

  updateStats(cycle);

  return packetLatency;
}

void IdealDRAM::postDone(Request *req) {
  scheduleNow(req->eid, req->data);

  delete req;
}

void IdealDRAM::read(uint64_t address, Event eid, uint64_t data) {
  auto req = new Request(true, address, eid, data);

  // Enqueue request
  req->beginAt = getTick();

  readStat.add(MemoryPacketSize);

  scheduler.enqueue(req);
}

void IdealDRAM::write(uint64_t address, Event eid, uint64_t data) {
  auto req = new Request(false, address, eid, data);

  // Enqueue request
  req->beginAt = getTick();

  writeStat.add(MemoryPacketSize);

  scheduler.enqueue(req);
}

void IdealDRAM::createCheckpoint(std::ostream &out) const noexcept {
  IdealDRAM::createCheckpoint(out);

  BACKUP_SCALAR(out, packetLatency);

  scheduler.createCheckpoint(out);
}

void IdealDRAM::restoreCheckpoint(std::istream &in) noexcept {
  IdealDRAM::restoreCheckpoint(in);

  RESTORE_SCALAR(in, packetLatency);

  scheduler.restoreCheckpoint(in);
}

void IdealDRAM::getStatList(std::vector<Stat> &list,
                            std::string prefix) noexcept {
  AbstractDRAM::getStatList(list, prefix);

  list.emplace_back(prefix + "energy",
                    "Total energy comsumed by embedded DRAM (pJ)");
  list.emplace_back(prefix + "power",
                    "Total power comsumed by embedded DRAM (mW)");
}

void IdealDRAM::getStatValues(std::vector<double> &values) noexcept {
  AbstractDRAM::getStatValues(values);

  values.push_back(totalEnergy);
  values.push_back(totalPower);
}

void IdealDRAM::resetStatValues() noexcept {
  AbstractDRAM::resetStatValues();

  // calcWindowEnergy clears old data
  dramPower->calcWindowEnergy(getTick() / pTiming->tCK);

  totalEnergy = 0.0;
  totalPower = 0.0;
}

void IdealDRAM::updateStats(uint64_t cycle) noexcept {
  dramPower->calcWindowEnergy(cycle);

  auto &energy = dramPower->getEnergy();
  auto &power = dramPower->getPower();

  totalEnergy += energy.window_energy;
  totalPower = power.average_power;
}

}  // namespace SimpleSSD::Memory::DRAM
