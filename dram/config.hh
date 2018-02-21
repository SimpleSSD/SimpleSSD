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

#ifndef __DRAM_CONFIG__
#define __DRAM_CONFIG__

#include "util/base_config.hh"

namespace SimpleSSD {

namespace DRAM {

typedef enum {
  DRAM_MODEL,
} DRAM_CONFIG;

typedef enum {
  SIMPLE_MODEL,
} MODEL;

class Config : public BaseConfig {
 public:
  typedef struct {
    uint32_t channel;
    uint32_t rank;
    uint32_t bank;
    uint32_t chip;
    uint32_t busWidth;
    uint32_t burstLength;
    uint32_t activationLimit;
    bool useDLL;
    uint64_t chipSize;
    uint64_t pageSize;
  } DRAMStructure;

  typedef struct {
    uint32_t tCK;
    uint32_t tRCD;
    uint32_t tCL;
    uint32_t tRP;
    uint32_t tRAS;
    uint32_t tWR;
    uint32_t tRTP;
    uint32_t tBURST;
    uint32_t tCCD_L;
    uint32_t tRFC;
    uint32_t tREFI;
    uint32_t tWTR;
    uint32_t tRTW;
    uint32_t tCS;
    uint32_t tRRD;
    uint32_t tRRD_L;
    uint32_t tXAW;
    uint32_t tXP;
    uint32_t tXPDLL;
    uint32_t tXS;
    uint32_t tXSDLL;
  } DRAMTiming;

  typedef struct {
    float pIDD0[2];
    float pIDD2P0[2];
    float pIDD2P1[2];
    float pIDD2N[2];
    float pIDD3P0[2];
    float pIDD3P1[2];
    float pIDD3N[2];
    float pIDD4R[2];
    float pIDD4W[2];
    float pIDD5[2];
    float pIDD6[2];
    float pVDD[2];
  } DRAMPower;

 private:
  MODEL model;

  DRAMStructure dram;
  DRAMTiming dramTiming;
  DRAMPower dramPower;

 public:
  Config();

  bool setConfig(const char *, const char *) override;

  int64_t readInt(uint32_t) override;

  DRAMStructure *getDRAMStructure();
  DRAMTiming *getDRAMTiming();
  DRAMPower *getDRAMPower();
};

}  // namespace DRAM

}  // namespace SimpleSSD

#endif
