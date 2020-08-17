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

  convertMemspec();

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

void IdealDRAM::convertMemspec() {
  // gem5 src/mem/drampower.cc
  spec.memArchSpec.burstLength = pStructure->burstLength;
  spec.memArchSpec.nbrOfBanks = pStructure->bank;
  spec.memArchSpec.nbrOfRanks = pStructure->rank;
  spec.memArchSpec.dataRate = 2;
  spec.memArchSpec.nbrOfColumns = 0;
  spec.memArchSpec.nbrOfRows = 0;
  spec.memArchSpec.width = pStructure->width;
  spec.memArchSpec.nbrOfBankGroups = 0;
  spec.memArchSpec.dll = false;
  spec.memArchSpec.twoVoltageDomains = pPower->pVDD[1] != 0.f;
  spec.memArchSpec.termination = false;

  spec.memTimingSpec.RC = DIVCEIL(pTiming->tRAS + pTiming->tRP, pTiming->tCK);
  spec.memTimingSpec.RCD = DIVCEIL(pTiming->tRCD, pTiming->tCK);
  spec.memTimingSpec.RL = DIVCEIL(pTiming->tRL, pTiming->tCK);
  spec.memTimingSpec.RP = DIVCEIL(pTiming->tRP, pTiming->tCK);
  spec.memTimingSpec.RFC = DIVCEIL(pTiming->tRFC, pTiming->tCK);
  spec.memTimingSpec.RAS = DIVCEIL(pTiming->tRAS, pTiming->tCK);
  spec.memTimingSpec.WL = DIVCEIL(pTiming->tWL, pTiming->tCK);
  spec.memTimingSpec.DQSCK = DIVCEIL(pTiming->tDQSCK, pTiming->tCK);
  spec.memTimingSpec.RTP = DIVCEIL(pTiming->tRTP, pTiming->tCK);
  spec.memTimingSpec.WR = DIVCEIL(pTiming->tWR, pTiming->tCK);
  spec.memTimingSpec.XP = 0;
  spec.memTimingSpec.XPDLL = 0;
  spec.memTimingSpec.XS = 0;
  spec.memTimingSpec.XSDLL = 0;
  spec.memTimingSpec.clkPeriod = pTiming->tCK / 1000.;

  if (spec.memTimingSpec.clkPeriod == 0) {
    panic("Invalid DRAM clock period");
  }

  spec.memTimingSpec.clkMhz = (1 / spec.memTimingSpec.clkPeriod) * 1000.;

  spec.memPowerSpec.idd0 = pPower->pIDD0[0];
  spec.memPowerSpec.idd02 = pPower->pIDD0[1];
  spec.memPowerSpec.idd2p0 = pPower->pIDD2P0[0];
  spec.memPowerSpec.idd2p02 = pPower->pIDD2P0[1];
  spec.memPowerSpec.idd2p1 = pPower->pIDD2P1[0];
  spec.memPowerSpec.idd2p12 = pPower->pIDD2P1[1];
  spec.memPowerSpec.idd2n = pPower->pIDD2N[0];
  spec.memPowerSpec.idd2n2 = pPower->pIDD2N[1];
  spec.memPowerSpec.idd3p0 = pPower->pIDD3P0[0];
  spec.memPowerSpec.idd3p02 = pPower->pIDD3P0[1];
  spec.memPowerSpec.idd3p1 = pPower->pIDD3P1[0];
  spec.memPowerSpec.idd3p12 = pPower->pIDD3P1[1];
  spec.memPowerSpec.idd3n = pPower->pIDD3N[0];
  spec.memPowerSpec.idd3n2 = pPower->pIDD3N[1];
  spec.memPowerSpec.idd4r = pPower->pIDD4R[0];
  spec.memPowerSpec.idd4r2 = pPower->pIDD4R[1];
  spec.memPowerSpec.idd4w = pPower->pIDD4W[0];
  spec.memPowerSpec.idd4w2 = pPower->pIDD4W[1];
  spec.memPowerSpec.idd5 = pPower->pIDD5[0];
  spec.memPowerSpec.idd52 = pPower->pIDD5[1];
  spec.memPowerSpec.idd6 = pPower->pIDD6[0];
  spec.memPowerSpec.idd62 = pPower->pIDD6[1];
  spec.memPowerSpec.vdd = pPower->pVDD[0];
  spec.memPowerSpec.vdd2 = pPower->pVDD[1];
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
