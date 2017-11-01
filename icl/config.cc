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

#include "util/algorithm.hh"

namespace SimpleSSD {

namespace ICL {

const char NAME_USE_READ_CACHE[] = "EnableReadCache";
const char NAME_USE_WRITE_CACHE[] = "EnableWriteCache";
const char NAME_USE_READ_PREFETCH[] = "EnableReadPrefetch";
const char NAME_EVICT_POLICY[] = "EvictPolicy";
const char NAME_SET_SIZE[] = "CacheSetSize";
const char NAME_ENTRY_SIZE[] = "CacheEntrySize";

Config::Config() {
  readCaching = false;
  writeCaching = true;
  readPrefetch = false;
  evictPolicy = POLICY_LEAST_RECENTLY_USED;
  cacheSetSize = 8192;
  cacheEntrySize = 1;

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
  else if (MATCH_NAME(NAME_EVICT_POLICY)) {
    evictPolicy = (EVICT_POLICY)strtoul(value, nullptr, 10);
  }
  else if (MATCH_NAME(NAME_SET_SIZE)) {
    cacheSetSize = strtoul(value, nullptr, 10);
  }
  else if (MATCH_NAME(NAME_ENTRY_SIZE)) {
    cacheEntrySize = strtoul(value, nullptr, 10);
  }
  else {
    ret = false;
  }

  return ret;
}

void Config::update() {
  if (popcount(cacheSetSize) != 1) {
    // TODO: panic("cache set size should be power of 2");
  }
  if (popcount(cacheEntrySize) != 1) {
    // TODO: panic("cache entry size should be power of 2");
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
    case ICL_SET_SIZE:
      ret = cacheSetSize;
      break;
    case ICL_ENTRY_SIZE:
      ret = cacheEntrySize;
      break;
  }

  return ret;
}

float Config::readFloat(uint32_t idx) {
  float ret = 0.f;

  return ret;
}

std::string Config::readString(uint32_t idx) {
  std::string ret("");

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
