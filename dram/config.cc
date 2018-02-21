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

#include "dram/config.hh"

#include "log/trace.hh"
#include "util/algorithm.hh"

namespace SimpleSSD {

namespace DRAM {

const char NAME_DRAM_MODEL[] = "DRAMModel";
const char NAME_DRAM_CHANNEL[] = "DRAMChannel";
const char NAME_DRAM_BUS_WIDTH[] = "DRAMBusWidth";
const char NAME_DRAM_PAGE_SIZE[] = "DRAMPageSize";
const char NAME_DRAM_TIMING_CK[] = "DRAMtCK";
const char NAME_DRAM_TIMING_RCD[] = "DRAMtRCD";
const char NAME_DRAM_TIMING_CL[] = "DRAMtCL";
const char NAME_DRAM_TIMING_RP[] = "DRAMtRP";

Config::Config() {
  model = SIMPLE_MODEL;

  /* LPDDR3-1600 4Gbit 1x32 */
  dram.channel = 1;
  dram.rank = 1;
  dram.bank = 8;
  dram.chip = 1;
  dram.chipSize = 536870912;
  dram.busWidth = 32;
  dram.burstLength = 8;
  dram.activationLimit = 4;
  dram.useDLL = false;
  dram.pageSize = 4096;

  dramTiming.tCK = 1250;
  dramTiming.tRCD = 18000;
  dramTiming.tCL = 15000;
  dramTiming.tRP = 18000;
  dramTiming.tRAS = 42000;
  dramTiming.tWR = 15000;
  dramTiming.tRTP = 7500;
  dramTiming.tBURST = 5000;
  dramTiming.tCCD_L = 0;
  dramTiming.tRFC = 130000;
  dramTiming.tREFI = 3900;
  dramTiming.tWTR = 7500;
  dramTiming.tRTW = 2500;
  dramTiming.tCS = 2500;
  dramTiming.tRRD = 10000;
  dramTiming.tRRD_L = 0;
  dramTiming.tXAW = 50000;
  dramTiming.tXP = 0;
  dramTiming.tXPDLL = 0;
  dramTiming.tXS = 0;
  dramTiming.tXSDLL = 0;

  dramPower.pIDD0[0] = 8.f;
  dramPower.pIDD0[1] = 60.f;
  dramPower.pIDD2P0[0] = 0.f;
  dramPower.pIDD2P0[1] = 0.f;
  dramPower.pIDD2P1[0] = 0.8f;
  dramPower.pIDD2P1[1] = 1.8f;
  dramPower.pIDD2N[0] = 0.8f;
  dramPower.pIDD2N[1] = 26.f;
  dramPower.pIDD3P0[0] = 0.f;
  dramPower.pIDD3P0[1] = 0.f;
  dramPower.pIDD3P1[0] = 1.4f;
  dramPower.pIDD3P1[1] = 11.f;
  dramPower.pIDD3N[0] = 2.f;
  dramPower.pIDD3N[1] = 34.f;
  dramPower.pIDD4R[0] = 2.f;
  dramPower.pIDD4R[1] = 230.f;
  dramPower.pIDD4W[0] = 2.f;
  dramPower.pIDD4W[1] = 190.f;
  dramPower.pIDD5[0] = 28.f;
  dramPower.pIDD5[1] = 150.f;
  dramPower.pIDD6[0] = 0.5f;
  dramPower.pIDD6[1] = 1.8f;
  dramPower.pVDD[0] = 1.8f;
  dramPower.pVDD[1] = 1.2f;
}

bool Config::setConfig(const char *name, const char *value) {
  bool ret = true;

  if (MATCH_NAME(NAME_DRAM_MODEL)) {
    model = (MODEL)strtoul(value, nullptr, 10);
  }
  else if (MATCH_NAME(NAME_DRAM_CHANNEL)) {
    dram.channel = strtoul(value, nullptr, 10);
  }
  else if (MATCH_NAME(NAME_DRAM_BUS_WIDTH)) {
    dram.busWidth = strtoul(value, nullptr, 10);
  }
  else if (MATCH_NAME(NAME_DRAM_PAGE_SIZE)) {
    dram.pageSize = strtoul(value, nullptr, 10);
  }
  else if (MATCH_NAME(NAME_DRAM_TIMING_CK)) {
    dramTiming.tCK = strtoul(value, nullptr, 10);
  }
  else if (MATCH_NAME(NAME_DRAM_TIMING_RCD)) {
    dramTiming.tRCD = strtoul(value, nullptr, 10);
  }
  else if (MATCH_NAME(NAME_DRAM_TIMING_CL)) {
    dramTiming.tCL = strtoul(value, nullptr, 10);
  }
  else if (MATCH_NAME(NAME_DRAM_TIMING_RP)) {
    dramTiming.tRP = strtoul(value, nullptr, 10);
  }
  else {
    ret = false;
  }

  return ret;
}

int64_t Config::readInt(uint32_t idx) {
  int64_t ret = 0;

  switch (idx) {
    case DRAM_MODEL:
      ret = model;
      break;
  }

  return ret;
}

Config::DRAMStructure *Config::getDRAMStructure() {
  return &dram;
}

Config::DRAMTiming *Config::getDRAMTiming() {
  return &dramTiming;
}

Config::DRAMPower *Config::getDRAMPower() {
  return &dramPower;
}

}  // namespace DRAM

}  // namespace SimpleSSD
