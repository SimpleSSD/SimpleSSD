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

  for (uint8_t i = 0; i < pStructure->rank; i++) {
    dramPower.emplace_back(libDRAMPower(spec, false));
  }

  powerStat.resize(pStructure->rank);

  for (auto &stat : powerStat) {
    stat.clear();
  }
}

AbstractDRAM::~AbstractDRAM() {}

void AbstractDRAM::getStatList(std::vector<Stat> &list,
                               std::string prefix) noexcept {
  for (uint8_t rank = 0; rank < pStructure->rank; rank++) {
    std::string rprefix = prefix + "rank" + std::to_string(rank) + ".";

    list.emplace_back(rprefix + "energy.activate",
                      "Energy for activate commands per rank (pJ)");
    list.emplace_back(rprefix + "energy.precharge",
                      "Energy for precharge commands per rank (pJ)");
    list.emplace_back(rprefix + "energy.read",
                      "Energy for read commands per rank (pJ)");
    list.emplace_back(rprefix + "energy.write",
                      "Energy for write commands per rank (pJ)");
    list.emplace_back(rprefix + "energy.refresh",
                      "Energy for refresh commands per rank (pJ)");
    list.emplace_back(rprefix + "energy.self_refresh",
                      "Energy for self refresh per rank (pJ)");
    list.emplace_back(rprefix + "energy.total", "Total energy per rank (pJ)");
    list.emplace_back(rprefix + "power", "Core power per rank (mW)");
  }
}

void AbstractDRAM::getStatValues(std::vector<double> &values) noexcept {
  // As calcWindowEnergy resets previous power/energy stat,
  // we need to store previous values.
  for (uint8_t rank = 0; rank < pStructure->rank; rank++) {
    auto &power = dramPower.at(rank);
    auto &stat = powerStat.at(rank);

    power.calcWindowEnergy(getTick() / pTiming->tCK);

    auto &energy = power.getEnergy();

    stat.act_energy += energy.act_energy * pStructure->chip;
    stat.pre_energy += energy.pre_energy * pStructure->chip;
    stat.read_energy += energy.read_energy * pStructure->chip;
    stat.write_energy += energy.write_energy * pStructure->chip;
    stat.ref_energy += energy.ref_energy * pStructure->chip;
    stat.sref_energy += energy.sref_energy * pStructure->chip;
    stat.window_energy += energy.window_energy * pStructure->chip;

    values.push_back(stat.act_energy);
    values.push_back(stat.pre_energy);
    values.push_back(stat.read_energy);
    values.push_back(stat.write_energy);
    values.push_back(stat.ref_energy);
    values.push_back(stat.sref_energy);
    values.push_back(stat.window_energy);
    values.push_back(1000.0 * stat.window_energy / (getTick() - lastResetAt));
  }
}

void AbstractDRAM::resetStatValues() noexcept {
  for (auto &power : dramPower) {
    power.calcWindowEnergy(getTick() / pTiming->tCK);
  }

  for (auto &stat : powerStat) {
    stat.clear();
  }

  lastResetAt = getTick();
}

void AbstractDRAM::createCheckpoint(std::ostream &out) const noexcept {
  BACKUP_SCALAR(out, lastResetAt);
}

void AbstractDRAM::restoreCheckpoint(std::istream &in) noexcept {
  RESTORE_SCALAR(in, lastResetAt);
}

}  // namespace SimpleSSD::Memory::DRAM
