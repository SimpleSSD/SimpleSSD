// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_MEM_CONFIG_HH__
#define __SIMPLESSD_MEM_CONFIG_HH__

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
    Gem5,
    Full = 1,
  };

  enum class MemoryScheduling : uint8_t {
    FCFS,
    FRFCFS,
  };

  enum class AddressMapping : uint8_t {
    RoRaBaChCo,
    RoRaBaCoCh,
    RoCoRaBaCh,
  };

  enum class PagePolicy : uint8_t {
    Open,
    OpenAdaptive,
    Close,
    CloseAdaptive,
  };

  //! SRAM structure parameters
  struct SRAMStructure {
    uint64_t size;
    uint64_t lineSize;
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
    uint32_t tCK;        //!< Clock period
    uint32_t tRCD;       //!< RAS to CAS delay
    uint32_t tCL;        //!< CAS latency
    uint32_t tRP;        //!< Row precharge time
    uint32_t tRAS;       //!< ACT to PRE delay
    uint32_t tWR;        //!< Write recovery time
    uint32_t tRTP;       //!< Read to precharge delay
    uint32_t tBURST;     //!< Burst duration
    uint32_t tCCD_L;     //!< Same bank group CAS to CAS delay
    uint32_t tCCD_L_WR;  //!< Same bank group write to writ delay
    uint32_t tRFC;       //!< Refresh cycle time
    uint32_t tREFI;      //!< Refresh command interval
    uint32_t tWTR;       //!< Write to read, same rank switching time
    uint32_t tRTW;       //!< Read to write, same rank switching time
    uint32_t tCS;        //!< Rank to rank switching time
    uint32_t tRRD;       //!< ACT to ACT delay
    uint32_t tRRD_L;     //!< Same bank group RRD
    uint32_t tXAW;       //!< X activation window
    uint32_t tXP;        //!< Power-up delay
    uint32_t tXPDLL;     //!< Power-up delay with locked DLL
    uint32_t tXS;        //!< Self-refresh exit latency
    uint32_t tXSDLL;     //!< Self-refresh exit latency DLL
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

  //! TimingDRAM parameters
  struct TimingDRAMConfig {
    uint32_t writeBufferSize;
    uint32_t readBufferSize;
    float forceWriteThreshold;
    float startWriteThreshold;
    uint32_t minWriteBurst;
    MemoryScheduling scheduling;
    AddressMapping mapping;
    PagePolicy policy;
    uint64_t frontendLatency;
    uint64_t backendLatency;
    uint32_t maxAccessesPerRow;
    uint32_t rowBufferSize;
    uint32_t bankGroup;
    bool enablePowerdown;
    bool useDLL;
  };

 private:
  SRAMStructure sram;
  DRAMStructure dram;
  DRAMTiming timing;
  DRAMPower power;
  TimingDRAMConfig gem5;

  Model dramModel;

  void loadSRAM(pugi::xml_node &);
  void loadDRAMStructure(pugi::xml_node &);
  void loadDRAMTiming(pugi::xml_node &);
  void loadDRAMPower(pugi::xml_node &);
  void loadTimingDRAM(pugi::xml_node &);
  void storeSRAM(pugi::xml_node &);
  void storeDRAMStructure(pugi::xml_node &);
  void storeDRAMTiming(pugi::xml_node &);
  void storeDRAMPower(pugi::xml_node &);
  void storeTimingDRAM(pugi::xml_node &);

 public:
  Config();
  ~Config();

  const char *getSectionName() override { return "memory"; }

  void loadFrom(pugi::xml_node &) override;
  void storeTo(pugi::xml_node &) override;
  void update() override;

  uint64_t readUint(uint32_t) override;
  bool writeUint(uint32_t, uint64_t) override;

  SRAMStructure *getSRAM();
  DRAMStructure *getDRAM();
  DRAMTiming *getDRAMTiming();
  DRAMPower *getDRAMPower();
  TimingDRAMConfig *getTimingDRAM();
};

}  // namespace SimpleSSD::Memory

#endif
