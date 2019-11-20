// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "mem/dram/abstract_dram.hh"

#include <cstring>

#include "util/algorithm.hh"

namespace SimpleSSD::Memory::DRAM {

AbstractDRAM::AbstractDRAM(ObjectData &o) : Object(o) {
  pStructure = o.config->getDRAM();
  pTiming = o.config->getDRAMTiming();
  pPower = o.config->getDRAMPower();

  // Reset stats
  lastResetAt = 0;
  act_energy = 0;
  pre_energy = 0;
  read_energy = 0;
  write_energy = 0;
  ref_energy = 0;
  sref_energy = 0;
  window_energy = 0;

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

  spec.memTimingSpec.RC = DIVCEIL(pTiming->tRRD, pTiming->tCK);
  spec.memTimingSpec.RCD = DIVCEIL(pTiming->tRCD, pTiming->tCK);
  spec.memTimingSpec.RL = DIVCEIL(pTiming->tRL, pTiming->tCK);
  spec.memTimingSpec.RP = DIVCEIL(pTiming->tRP, pTiming->tCK);
  spec.memTimingSpec.RFC = DIVCEIL(pTiming->tRFC, pTiming->tCK);
  spec.memTimingSpec.RAS = spec.memTimingSpec.RRD - spec.memTimingSpec.RP;
  spec.memTimingSpec.WL = DIVCEIL(pTiming->tWL, pTiming->tCK);
  spec.memTimingSpec.DQSCK = DIVCEIL(pTiming->tDQSCK, pTiming->tCK);
  spec.memTimingSpec.RTP = DIVCEIL(pTiming->tRTP, pTiming->tCK);
  spec.memTimingSpec.WR = DIVCEIL(pTiming->tWR, pTiming->tCK);
  spec.memTimingSpec.XS = DIVCEIL(pTiming->tSR, pTiming->tCK);
  spec.memTimingSpec.clkPeriod = pTiming->tCK / 1000.;

  if (spec.memTimingSpec.clkPeriod == 0.) {
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

  dramPower = new libDRAMPower(spec, false);
}

AbstractDRAM::~AbstractDRAM() {
  delete dramPower;
}

void AbstractDRAM::getStatList(std::vector<Stat> &list,
                               std::string prefix) noexcept {
  list.emplace_back(prefix + "request_count.read", "Read request count");
  list.emplace_back(prefix + "request_count.write", "Write request count");
  list.emplace_back(prefix + "request_count.total", "Total request count");
  list.emplace_back(prefix + "bytes.read", "Read data size in byte");
  list.emplace_back(prefix + "bytes.write", "Write data size in byte");
  list.emplace_back(prefix + "bytes.total", "Total data size in byte");

  list.emplace_back(prefix + "energy.activate",
                    "Energy for activate commands per rank (pJ)");
  list.emplace_back(prefix + "energy.precharge",
                    "Energy for precharge commands per rank (pJ)");
  list.emplace_back(prefix + "energy.read",
                    "Energy for read commands per rank (pJ)");
  list.emplace_back(prefix + "energy.write",
                    "Energy for write commands per rank (pJ)");
  list.emplace_back(prefix + "energy.refresh",
                    "Energy for refresh commands per rank (pJ)");
  list.emplace_back(prefix + "energy.self_refresh",
                    "Energy for self refresh per rank (pJ)");
  list.emplace_back(prefix + "energy.total", "Total energy per rank (pJ)");
  list.emplace_back(prefix + "power", "Core power per rank (mW)");
}

void AbstractDRAM::getStatValues(std::vector<double> &values) noexcept {
  values.push_back((double)readStat.count);
  values.push_back((double)writeStat.count);
  values.push_back((double)(readStat.count + writeStat.count));
  values.push_back((double)readStat.size);
  values.push_back((double)writeStat.size);
  values.push_back((double)(readStat.size + writeStat.size));

  // As calcWindowEnergy resets previous power/energy stat,
  // we need to store previous values.
  dramPower->calcWindowEnergy(getTick() / pTiming->tCK);

  auto &energy = dramPower->getEnergy();

  act_energy += energy.act_energy * pStructure->chip;
  pre_energy += energy.pre_energy * pStructure->chip;
  read_energy += energy.read_energy * pStructure->chip;
  write_energy += energy.write_energy * pStructure->chip;
  ref_energy += energy.ref_energy * pStructure->chip;
  sref_energy += energy.sref_energy * pStructure->chip;
  window_energy += energy.window_energy * pStructure->chip;

  values.push_back(act_energy);
  values.push_back(pre_energy);
  values.push_back(read_energy);
  values.push_back(write_energy);
  values.push_back(ref_energy);
  values.push_back(sref_energy);
  values.push_back(window_energy);
  values.push_back(1000.0 * window_energy / (getTick() - lastResetAt));
}

void AbstractDRAM::resetStatValues() noexcept {
  dramPower->calcWindowEnergy(getTick() / pTiming->tCK);

  act_energy = 0.;
  pre_energy = 0.;
  read_energy = 0.;
  write_energy = 0.;
  ref_energy = 0.;
  sref_energy = 0.;
  window_energy = 0.;

  lastResetAt = getTick();

  readStat.clear();
  writeStat.clear();
}

void AbstractDRAM::createCheckpoint(std::ostream &out) const noexcept {
  BACKUP_SCALAR(out, readStat);
  BACKUP_SCALAR(out, writeStat);
}

void AbstractDRAM::restoreCheckpoint(std::istream &in) noexcept {
  RESTORE_SCALAR(in, readStat);
  RESTORE_SCALAR(in, writeStat);
}

}  // namespace SimpleSSD::Memory::DRAM
