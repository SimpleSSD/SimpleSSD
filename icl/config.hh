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

#ifndef __ICL_CONFIG__
#define __ICL_CONFIG__

#include "util/base_config.hh"

namespace SimpleSSD {

namespace ICL {

typedef enum {
  /* Cache config */
  ICL_USE_READ_CACHE,
  ICL_USE_WRITE_CACHE,
  ICL_USE_READ_PREFETCH,
  ICL_EVICT_POLICY,
  ICL_SET_SIZE,
  ICL_WAY_SIZE,

  /* DRAM config for dram_ctrl TODO: seperate this */
  /* Structure */
  DRAM_CHANNEL,
  DRAM_RANK,              //!< Ranks / Channel
  DRAM_BANK,              //!< Banks / Rank
  DRAM_CHIP,              //!< Chips / Rank
  DRAM_CHIP_SIZE,         //!< Chip size in bytes
  DRAM_CHIP_BUS_WIDTH,    //!< Bus width / Channel in bits
  DRAM_DLL,               //!< DLL is enabled
  DRAM_BURST_LENGTH,      //!< Burst length
  DRAM_ACTIVATION_LIMIT,  //!< Max number of activations in window
  DRAM_PAGE_SIZE,         //!< Page size of DRAM (Typically 4KB)

  /* Timing in ps */
  DRAM_TIMING_CK,     //!< Clock period
  DRAM_TIMING_RCD,    //!< RAS to CAS delay
  DRAM_TIMING_CL,     //!< CAS latency
  DRAM_TIMING_RP,     //!< Row Precharging time
  DRAM_TIMING_RAS,    //!< ACT to PRE delay
  DRAM_TIMING_WR,     //!< Write recovery time
  DRAM_TIMING_RTP,    //!< Read to precharge
  DRAM_TIMING_BURST,  //!< Burst duration
  DRAM_TIMING_CCD_L,  //!< Same bank group CAS to CAS delay
  DRAM_TIMING_RFC,    //!< Refrech cycle time
  DRAM_TIMING_REFI,   //!< Refresh command interval
  DRAM_TIMING_WTR,    //!< Write to read, same rank switching time
  DRAM_TIMING_RTW,    //!< Read to write, same rank switching time
  DRAM_TIMING_CS,     //!< Rank to rank switching time
  DRAM_TIMING_RRD,    //!< ACT to ACT delay
  DRAM_TIMING_RRD_L,  //!< Same bank group ACT to ACT delay
  DRAM_TIMING_XAW,    //!< X activation window
  DRAM_TIMING_XP,     //!< Power-up delay
  DRAM_TIMING_XPDLL,  //!< Power-up delay with locked DLL
  DRAM_TIMING_XS,     //!< Self-refresh exit latency
  DRAM_TIMING_XSDLL,  //!< Self-refresh exit latency with locked DLL

  /* Power in mA or V */
  DRAM_POWER_IDD0,     //!< Active precharge current
  DRAM_POWER_IDD02,    //!< Active precharge current VDD2
  DRAM_POWER_IDD2P0,   //!< Precharge powerdown slow
  DRAM_POWER_IDD2P02,  //!< Precharge powerdown slow VDD2
  DRAM_POWER_IDD2P1,   //!< Precharge powerdown fast
  DRAM_POWER_IDD2P12,  //!< Precharge powerdown fast VDD2
  DRAM_POWER_IDD2N,    //!< Precharge standby current
  DRAM_POWER_IDD2N2,   //!< Precharge standby current VDD2
  DRAM_POWER_IDD3P0,   //!< Active powerdown slow
  DRAM_POWER_IDD3P02,  //!< Active powerdown slow VDD2
  DRAM_POWER_IDD3P1,   //!< Active powerdown fast
  DRAM_POWER_IDD3P12,  //!< Active powerdown fast VDD2
  DRAM_POWER_IDD3N,    //!< Active standby current
  DRAM_POWER_IDD3N2,   //!< Active standby current VDD2
  DRAM_POWER_IDD4R,    //!< READ current
  DRAM_POWER_IDD4R2,   //!< READ current VDD2
  DRAM_POWER_IDD4W,    //!< WRITE current
  DRAM_POWER_IDD4W2,   //!< WRITE current VDD2
  DRAM_POWER_IDD5,     //!< Refresh current
  DRAM_POWER_IDD52,    //!< Refresh current VDD2
  DRAM_POWER_IDD6,     //!< Self-refresh current
  DRAM_POWER_IDD62,    //!< Self-refresh current VDD2
  DRAM_POWER_VDD,      //!< Main voltage
  DRAM_POWER_VDD2      //!< Second voltage
} ICL_CONFIG;

typedef enum {
  POLICY_RANDOM,               //!< Select way in random
  POLICY_FIFO,                 //!< Select way that lastly inserted
  POLICY_LEAST_RECENTLY_USED,  //!< Select way that least recently used
} EVICT_POLICY;

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
  bool readCaching;          //!< Default: false
  bool writeCaching;         //!< Default: true
  bool readPrefetch;         //!< Default: false
  EVICT_POLICY evictPolicy;  //!< Default: POLICY_LEAST_RECENTLY_USED
  uint64_t cacheSetSize;     //!< Default: 8192
  uint64_t cacheWaySize;     //!< Default: 1

  DRAMStructure dram;
  DRAMTiming dramTiming;
  DRAMPower dramPower;

 public:
  Config();

  bool setConfig(const char *, const char *);
  void update();

  int64_t readInt(uint32_t);
  uint64_t readUint(uint32_t);
  float readFloat(uint32_t);
  std::string readString(uint32_t);
  bool readBoolean(uint32_t);

  DRAMStructure *getDRAMStructure();
  DRAMTiming *getDRAMTiming();
  DRAMPower *getDRAMPower();
};

}  // namespace ICL

}  // namespace SimpleSSD

#endif
