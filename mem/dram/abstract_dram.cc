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

AbstractDRAM::AbstractDRAM(ObjectData &o)
    : Object(o), totalEnergy(0.0), totalPower(0.0) {
  pStructure = config->getDRAM();
  pTiming = config->getDRAMTiming();
  pPower = config->getDRAMPower();

  convertMemspec();

  dramPower = new libDRAMPower(spec, false);
}

AbstractDRAM::~AbstractDRAM() {
  delete dramPower;
}

void AbstractDRAM::convertMemspec() {
  // Calculate datarate
  uint32_t burstCycle = DIVCEIL(pTiming->tBURST, pTiming->tCK);
  uint8_t dataRate = pStructure->burst / burstCycle;

  panic_if(dataRate != 1 && dataRate != 2 && dataRate != 4,
           "Invalid DRAM data rate: %d", dataRate);

  // gem5 src/mem/drampower.cc
  spec.memArchSpec.burstLength = pStructure->burst;
  spec.memArchSpec.nbrOfBanks = pStructure->bank;
  spec.memArchSpec.nbrOfRanks = pStructure->rank;
  spec.memArchSpec.dataRate = dataRate;
  spec.memArchSpec.nbrOfColumns = 0;
  spec.memArchSpec.nbrOfRows = 0;
  spec.memArchSpec.width = pStructure->width;
  spec.memArchSpec.nbrOfBankGroups = 0;
  spec.memArchSpec.dll = pStructure->useDLL;
  spec.memArchSpec.twoVoltageDomains = pPower->pVDD[1] != 0.f;
  spec.memArchSpec.termination = false;

  spec.memTimingSpec.RC = DIVCEIL(pTiming->tRAS + pTiming->tRP, pTiming->tCK);
  spec.memTimingSpec.RCD = DIVCEIL(pTiming->tRCD, pTiming->tCK);
  spec.memTimingSpec.RL = DIVCEIL(pTiming->tCL, pTiming->tCK);
  spec.memTimingSpec.RP = DIVCEIL(pTiming->tRP, pTiming->tCK);
  spec.memTimingSpec.RFC = DIVCEIL(pTiming->tRFC, pTiming->tCK);
  spec.memTimingSpec.RAS = DIVCEIL(pTiming->tRAS, pTiming->tCK);
  spec.memTimingSpec.WL = spec.memTimingSpec.RL - 1;
  spec.memTimingSpec.DQSCK = 0;
  spec.memTimingSpec.RTP = DIVCEIL(pTiming->tRTP, pTiming->tCK);
  spec.memTimingSpec.WR = DIVCEIL(pTiming->tWR, pTiming->tCK);
  spec.memTimingSpec.XP = DIVCEIL(pTiming->tXP, pTiming->tCK);
  spec.memTimingSpec.XPDLL = DIVCEIL(pTiming->tXPDLL, pTiming->tCK);
  spec.memTimingSpec.XS = DIVCEIL(pTiming->tXS, pTiming->tCK);
  spec.memTimingSpec.XSDLL = DIVCEIL(pTiming->tXSDLL, pTiming->tCK);
  spec.memTimingSpec.clkPeriod = pTiming->tCK / 1000.;

  panic_if(spec.memTimingSpec.clkPeriod == 0, "Invalid DRAM clock period");

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

void AbstractDRAM::updateStats(uint64_t cycle) {
  dramPower->calcWindowEnergy(cycle);

  auto &energy = dramPower->getEnergy();
  auto &power = dramPower->getPower();

  totalEnergy += energy.window_energy;
  totalPower = power.average_power;
}

void AbstractDRAM::getStatList(std::vector<Stat> &list, std::string prefix) {
  Stat temp;

  temp.name = prefix + "energy";
  temp.desc = "Total energy comsumed by embedded DRAM (pJ)";
  list.push_back(temp);

  temp.name = prefix + "power";
  temp.desc = "Total power comsumed by embedded DRAM (mW)";
  list.push_back(temp);
}

void AbstractDRAM::getStatValues(std::vector<double> &values) {
  values.push_back(totalEnergy);
  values.push_back(totalPower);
}

void AbstractDRAM::resetStatValues() {
  // calcWindowEnergy clears old data
  dramPower->calcWindowEnergy(getTick() / pTiming->tCK);

  totalEnergy = 0.0;
  totalPower = 0.0;
}

}  // namespace SimpleSSD::Memory::DRAM
