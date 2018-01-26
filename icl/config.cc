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

#include "icl/config.hh"

#include "log/trace.hh"
#include "util/algorithm.hh"

namespace SimpleSSD {

namespace ICL {

const char NAME_USE_READ_CACHE[] = "EnableReadCache";
const char NAME_USE_WRITE_CACHE[] = "EnableWriteCache";
const char NAME_USE_READ_PREFETCH[] = "EnableReadPrefetch";
const char NAME_EVICT_POLICY[] = "EvictPolicy";
const char NAME_CACHE_SIZE[] = "CacheSize";
const char NAME_WAY_SIZE[] = "CacheWaySize";
const char NAME_PREFETCH_COUNT[] = "ReadPrefetchCount";
const char NAME_PREFETCH_RATIO[] = "ReadPrefetchRatio";

// TODO: seperate This
const char NAME_DRAM_CHANNEL[] = "DRAMChannel";
const char NAME_DRAM_BUS_WIDTH[] = "DRAMBusWidth";
const char NAME_DRAM_PAGE_SIZE[] = "DRAMPageSize";
const char NAME_DRAM_TIMING_CK[] = "DRAMtCK";
const char NAME_DRAM_TIMING_RCD[] = "DRAMtRCD";
const char NAME_DRAM_TIMING_CL[] = "DRAMtCL";
const char NAME_DRAM_TIMING_RP[] = "DRAMtRP";

Config::Config() {
  readCaching = false;
  writeCaching = true;
  readPrefetch = false;
  evictPolicy = POLICY_LEAST_RECENTLY_USED;
  cacheSize = 33554432;
  cacheWaySize = 1;
  prefetchCount = 1;
  prefetchRatio = 0.5;

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

  if (MATCH_NAME(NAME_USE_READ_CACHE)) {
    readCaching = convertBool(value);
  }
  else if (MATCH_NAME(NAME_USE_WRITE_CACHE)) {
    writeCaching = convertBool(value);
  }
  else if (MATCH_NAME(NAME_USE_READ_PREFETCH)) {
    readPrefetch = convertBool(value);
  }
  else if (MATCH_NAME(NAME_PREFETCH_COUNT)) {
    prefetchCount = strtoul(value, nullptr, 10);
  }
  else if (MATCH_NAME(NAME_PREFETCH_RATIO)) {
    prefetchRatio = strtof(value, nullptr);
  }
  else if (MATCH_NAME(NAME_EVICT_POLICY)) {
    evictPolicy = (EVICT_POLICY)strtoul(value, nullptr, 10);
  }
  else if (MATCH_NAME(NAME_CACHE_SIZE)) {
    cacheSize = strtoul(value, nullptr, 10);
  }
  else if (MATCH_NAME(NAME_WAY_SIZE)) {
    cacheWaySize = strtoul(value, nullptr, 10);
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

void Config::update() {
  if (prefetchCount == 0) {
    Logger::panic("Invalid ReadPrefetchCount");
  }
  if (prefetchRatio <= 0.f) {
    Logger::panic("Invalid ReadPrefetchRatio");
  }
}

int64_t Config::readInt(uint32_t idx) {
  int64_t ret = 0;

  switch (idx) {
    case ICL_EVICT_POLICY:
      ret = evictPolicy;
      break;
  }

  return ret;
}

uint64_t Config::readUint(uint32_t idx) {
  uint64_t ret = 0;

  switch (idx) {
    case ICL_CACHE_SIZE:
      ret = cacheSize;
      break;
    case ICL_WAY_SIZE:
      ret = cacheWaySize;
      break;
    case ICL_PREFETCH_COUNT:
      ret = prefetchCount;
      break;
    case DRAM_CHANNEL:
      ret = dram.channel;
      break;
    case DRAM_RANK:
      ret = dram.rank;
      break;
    case DRAM_BANK:
      ret = dram.bank;
      break;
    case DRAM_CHIP:
      ret = dram.chip;
      break;
    case DRAM_CHIP_SIZE:
      ret = dram.chipSize;
      break;
    case DRAM_CHIP_BUS_WIDTH:
      ret = dram.busWidth;
      break;
    case DRAM_BURST_LENGTH:
      ret = dram.burstLength;
      break;
    case DRAM_ACTIVATION_LIMIT:
      ret = dram.activationLimit;
      break;
    case DRAM_PAGE_SIZE:
      ret = dram.pageSize;
      break;
    case DRAM_TIMING_CK:
      ret = dramTiming.tCK;
      break;
    case DRAM_TIMING_RCD:
      ret = dramTiming.tRCD;
      break;
    case DRAM_TIMING_CL:
      ret = dramTiming.tCL;
      break;
    case DRAM_TIMING_RP:
      ret = dramTiming.tRP;
      break;
    case DRAM_TIMING_RAS:
      ret = dramTiming.tRAS;
      break;
    case DRAM_TIMING_WR:
      ret = dramTiming.tWR;
      break;
    case DRAM_TIMING_RTP:
      ret = dramTiming.tRTP;
      break;
    case DRAM_TIMING_BURST:
      ret = dramTiming.tBURST;
      break;
    case DRAM_TIMING_CCD_L:
      ret = dramTiming.tCCD_L;
      break;
    case DRAM_TIMING_RFC:
      ret = dramTiming.tRFC;
      break;
    case DRAM_TIMING_REFI:
      ret = dramTiming.tREFI;
      break;
    case DRAM_TIMING_WTR:
      ret = dramTiming.tWTR;
      break;
    case DRAM_TIMING_RTW:
      ret = dramTiming.tRTW;
      break;
    case DRAM_TIMING_CS:
      ret = dramTiming.tCS;
      break;
    case DRAM_TIMING_RRD:
      ret = dramTiming.tRRD;
      break;
    case DRAM_TIMING_RRD_L:
      ret = dramTiming.tRRD_L;
      break;
    case DRAM_TIMING_XAW:
      ret = dramTiming.tXAW;
      break;
    case DRAM_TIMING_XP:
      ret = dramTiming.tXP;
      break;
    case DRAM_TIMING_XPDLL:
      ret = dramTiming.tXPDLL;
      break;
    case DRAM_TIMING_XS:
      ret = dramTiming.tXS;
      break;
    case DRAM_TIMING_XSDLL:
      ret = dramTiming.tXSDLL;
      break;
  }

  return ret;
}

float Config::readFloat(uint32_t idx) {
  float ret = 0.f;

  switch (idx) {
    case ICL_PREFETCH_RATIO:
      ret = prefetchRatio;
      break;
    case DRAM_POWER_IDD0:
      ret = dramPower.pIDD0[0];
      break;
    case DRAM_POWER_IDD02:
      ret = dramPower.pIDD0[1];
      break;
    case DRAM_POWER_IDD2P0:
      ret = dramPower.pIDD2P0[0];
      break;
    case DRAM_POWER_IDD2P02:
      ret = dramPower.pIDD2P0[1];
      break;
    case DRAM_POWER_IDD2P1:
      ret = dramPower.pIDD2P1[0];
      break;
    case DRAM_POWER_IDD2P12:
      ret = dramPower.pIDD2P1[1];
      break;
    case DRAM_POWER_IDD2N:
      ret = dramPower.pIDD2N[0];
      break;
    case DRAM_POWER_IDD2N2:
      ret = dramPower.pIDD2N[1];
      break;
    case DRAM_POWER_IDD3P0:
      ret = dramPower.pIDD3P0[0];
      break;
    case DRAM_POWER_IDD3P02:
      ret = dramPower.pIDD3P0[1];
      break;
    case DRAM_POWER_IDD3P1:
      ret = dramPower.pIDD3P1[0];
      break;
    case DRAM_POWER_IDD3P12:
      ret = dramPower.pIDD3P1[1];
      break;
    case DRAM_POWER_IDD3N:
      ret = dramPower.pIDD3N[0];
      break;
    case DRAM_POWER_IDD3N2:
      ret = dramPower.pIDD3N[1];
      break;
    case DRAM_POWER_IDD4R:
      ret = dramPower.pIDD4R[0];
      break;
    case DRAM_POWER_IDD4R2:
      ret = dramPower.pIDD4R[1];
      break;
    case DRAM_POWER_IDD4W:
      ret = dramPower.pIDD4W[0];
      break;
    case DRAM_POWER_IDD4W2:
      ret = dramPower.pIDD4W[1];
      break;
    case DRAM_POWER_IDD5:
      ret = dramPower.pIDD5[0];
      break;
    case DRAM_POWER_IDD52:
      ret = dramPower.pIDD5[1];
      break;
    case DRAM_POWER_IDD6:
      ret = dramPower.pIDD6[0];
      break;
    case DRAM_POWER_IDD62:
      ret = dramPower.pIDD6[1];
      break;
    case DRAM_POWER_VDD:
      ret = dramPower.pVDD[0];
      break;
    case DRAM_POWER_VDD2:
      ret = dramPower.pVDD[1];
      break;
  }

  return ret;
}

bool Config::readBoolean(uint32_t idx) {
  bool ret = false;

  switch (idx) {
    case ICL_USE_READ_CACHE:
      ret = readCaching;
      break;
    case ICL_USE_WRITE_CACHE:
      ret = writeCaching;
      break;
    case ICL_USE_READ_PREFETCH:
      ret = readPrefetch;
      break;
    case DRAM_DLL:
      ret = dram.useDLL;
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

}  // namespace ICL

}  // namespace SimpleSSD
