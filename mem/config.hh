// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __MEM_CONFIG_HH__
#define __MEM_CONFIG_HH__

#include "sim/base_config.hh"

namespace SimpleSSD::Memory {

/**
 * \brief Memory Config object declaration
 *
 * Stores DRAM and cache configurations.
 */
class Config : public BaseConfig {
 public:
  enum Key : uint32_t {
    DRAMModel,
  };

  enum Model : uint8_t {
    Simple,
    SetAssociative,
    Full = 1,
  };

  //! Cache structure parameters
  struct CacheParameter {
    Model model;
    uint8_t way;
    uint16_t lineSize;
    uint32_t set;
    uint64_t size;
    uint64_t latency;
  };

  //! DRAM structure parameters.
  struct DRAMStructure {
    uint8_t channel;  //!< # Channel
    uint8_t rank;     //!< # Rank / Channel
    uint8_t bank;     //!< # Bank / Rank
    uint8_t chip;     //!< # Chip / Rank
    uint16_t width;   //!< Bus width / Chip
    uint16_t burst;
    uint64_t chipSize;
    uint32_t pageSize;
    uint32_t useDLL : 1;
    uint32_t activationLimit : 31;
  };

  //! DRAM timing parameters. Unit is ps
  struct DRAMTiming {
    uint32_t tCK;     //!< Clock period
    uint32_t tRCD;    //!< RAS to CAS delay
    uint32_t tCL;     //!< CAS latency
    uint32_t tRP;     //!< Row precharge time
    uint32_t tRAS;    //!< ACT to PRE delay
    uint32_t tWR;     //!< Write recovery time
    uint32_t tRTP;    //!< Read to precharge delay
    uint32_t tBURST;  //!< Burst duration
    uint32_t tCCD_L;  //!< Same bank group CAS to CAS delay
    uint32_t tRFC;    //!< Refresh cycle time
    uint32_t tREFI;   //!< Refresh command interval
    uint32_t tWTR;    //!< Write to read, same rank switching time
    uint32_t tRTW;    //!< Read to write, same rank switching time
    uint32_t tCS;     //!< Rank to rank switching time
    uint32_t tRRD;    //!< ACT to ACT delay
    uint32_t tRRD_L;  //!< Same bank group RRD
    uint32_t tXAW;    //!< X activation window
    uint32_t tXP;     //!< Power-up delay
    uint32_t tXPDLL;  //!< Power-up delay with locked DLL
    uint32_t tXS;     //!< Self-refresh exit latency
    uint32_t tXSDLL;  //!< Self-refresh exit latency DLL
  };

  //! DRAM power parameters. Unit is mA
  struct DRAMPower {
    float pIDD0[2];    //!< Active precharge current
    float pIDD2P0[2];  //!< Precharge powerdown slow
    float pIDD2P1[2];  //!< Precharge powerdown fast
    float pIDD2N[2];   //!< Precharge standby current
    float pIDD3P0[2];  //!< Active powerdown slow
    float pIDD3P1[2];  //!< Active powerdown fast
    float pIDD3N[2];   //!< Active standby current
    float pIDD4R[2];   //!< READ current
    float pIDD4W[2];   //!< WRITE current
    float pIDD5[2];    //!< Refresh current
    float pIDD6[2];    //!< Self-refresh current
    float pVDD[2];     //!< Main voltage (Unit: V)
  };

 private:
  CacheParameter level1;
  CacheParameter level2;
  DRAMStructure dram;
  DRAMTiming timing;
  DRAMPower power;

  Model dramModel;

  void loadCache(pugi::xml_node &, CacheParameter *);
  void loadDRAMStructure(pugi::xml_node &, DRAMStructure *);
  void loadDRAMTiming(pugi::xml_node &, DRAMTiming *);
  void loadDRAMPower(pugi::xml_node &, DRAMPower *);
  void storeCache(pugi::xml_node &, CacheParameter *);
  void storeDRAMStructure(pugi::xml_node &, DRAMStructure *);
  void storeDRAMTiming(pugi::xml_node &, DRAMTiming *);
  void storeDRAMPower(pugi::xml_node &, DRAMPower *);

 public:
  Config();
  ~Config();

  const char *getSectionName() override { return "memory"; }

  void loadFrom(pugi::xml_node &) override;
  void storeTo(pugi::xml_node &) override;
  void update() override;

  uint64_t readUint(uint32_t) override;
  bool writeUint(uint32_t, uint64_t) override;

  CacheParameter *getLevel1();
  CacheParameter *getLevel2();
  DRAMStructure *getDRAM();
  DRAMTiming *getDRAMTiming();
  DRAMPower *getDRAMPower();
};

}  // namespace SimpleSSD::Memory

#endif
