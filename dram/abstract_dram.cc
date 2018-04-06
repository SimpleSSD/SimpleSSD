/*
 * Copyright (C) 2017 CAMELab
 *
 * This file is part of SimpleSSD.
 *
 * SimpleSSD is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * SimpleSSD is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with SimpleSSD.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "dram/abstract_dram.hh"

#include <cstring>

#include "util/algorithm.hh"

namespace SimpleSSD {

namespace DRAM {

AbstractDRAM::EnergeStat::EnergeStat() {
  memset(this, 0, sizeof(EnergeStat));
}

AbstractDRAM::AbstractDRAM(ConfigReader &c) : conf(c) {
  pStructure = conf.getDRAMStructure();
  pTiming = conf.getDRAMTiming();
  pPower = conf.getDRAMPower();

  convertMemspec();

  dramPower = new libDRAMPower(spec, false);
}

AbstractDRAM::~AbstractDRAM() {
  delete dramPower;
}

void AbstractDRAM::convertMemspec() {
  // Calculate datarate
  uint32_t burstCycle = DIVCEIL(pTiming->tBURST, pTiming->tCK);
  uint8_t dataRate = pStructure->burstLength / burstCycle;

  if (dataRate != 1 && dataRate != 2 && dataRate != 4) {
    panic("Invalid DRAM data rate: %d", dataRate);
  }

  // gem5 src/mem/drampower.cc
  spec.memArchSpec.burstLength = pStructure->burstLength;
  spec.memArchSpec.nbrOfBanks = pStructure->bank;
  spec.memArchSpec.nbrOfRanks = pStructure->rank;
  spec.memArchSpec.dataRate = dataRate;
  spec.memArchSpec.nbrOfColumns = 0;
  spec.memArchSpec.nbrOfRows = 0;
  spec.memArchSpec.width = pStructure->busWidth;
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

void AbstractDRAM::getStatList(std::vector<Stats> &list, std::string prefix) {
  Stats temp;

  temp.name = prefix + "energy.act";
  temp.desc = "ACT command energy (pJ)";
  list.push_back(temp);

  temp.name = prefix + "energy.pre";
  temp.desc = "PRE command energy (pJ)";
  list.push_back(temp);

  temp.name = prefix + "energy.rd";
  temp.desc = "RD command energy (pJ)";
  list.push_back(temp);

  temp.name = prefix + "energy.wr";
  temp.desc = "WR command energy (pJ)";
  list.push_back(temp);

  temp.name = prefix + "energy.act.standby";
  temp.desc = "ACT standby energy (pJ)";
  list.push_back(temp);

  temp.name = prefix + "energy.pre.standby";
  temp.desc = "PRE standby energy (pJ)";
  list.push_back(temp);

  temp.name = prefix + "energy.ref";
  temp.desc = "Refresh energy (pJ)";
  list.push_back(temp);
}

void AbstractDRAM::getStatValues(std::vector<double> &values) {
  values.push_back(stat.act);
  values.push_back(stat.pre);
  values.push_back(stat.read);
  values.push_back(stat.write);
  values.push_back(stat.actStandby);
  values.push_back(stat.preStandby);
  values.push_back(stat.refresh);
}

void AbstractDRAM::resetStatValues() {
  // calcWindowEnergy clears old data
  dramPower->calcWindowEnergy(getTick() / pTiming->tCK);

  memset(&stat, 0, sizeof(EnergeStat));
}

}  // namespace DRAM

}  // namespace SimpleSSD
