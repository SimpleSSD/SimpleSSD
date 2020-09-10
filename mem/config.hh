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
    SystemBusSpeed,
  };

  enum Model : uint8_t {
    Ideal,
    Simple,
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

  //! Cache parameters
  struct CacheConfig {
    uint32_t size;            //!< Cache size
    uint16_t way;             //!< Number of ways
    uint16_t tagCycles;       //!< Tag lookup latency
    uint16_t dataCycles;      //!< Data access latency
    uint16_t responseCycles;  //!< Miss return path latency
  };

  //! SRAM structure parameters
  struct SRAMStructure {
    uint32_t size;         //!< SRAM size
    uint16_t dataRate;     //!< Data / clock
    uint16_t dataWidth;    //!< Data bus width in bits
    uint64_t clockSpeed;   //!< Operating clock speed
    uint16_t readCycles;   //!< Read latency
    uint16_t writeCycles;  //!< Write latency
    float pIDD;            //!< Operating current in mA
    float pISB1;           //!< Idle/Powerdown current in mA
    float pVCC;            //!< Operating voltage in V
  };

  //! DRAM structure parameters.
  struct DRAMStructure {
    uint8_t channel;      //!< # Channel
    uint8_t rank;         //!< # Rank / Channel
    uint8_t bank;         //!< # Bank / Rank
    uint8_t chip;         //!< # Chip / Rank
    uint16_t width;       //!< Bus width / Chip
    uint8_t burstChop;    //!< Burst chop (or smaller BL)
    uint8_t burstLength;  //!< BL (or larger BL)
    uint64_t chipSize;    //!< bytes / Chip
    uint32_t rowSize;     //!< Row buffer size
  };

  //! DRAM timing parameters. Unit is ps
  struct DRAMTiming {
    uint32_t tCK;     //!< Clock period
    uint32_t tRAS;    //!< Row address strobe
    uint32_t tRRD;    //!< ACT to ACT delay
    uint32_t tRCD;    //!< RAS to CAS delay
    uint32_t tCCD;    //!< CAS to CAS delay
    uint32_t tRP;     //!< Row precharge (Per bank)
    uint32_t tRPab;   //!< Row precharge (All banks)
    uint32_t tRL;     //!< Read latency
    uint32_t tWL;     //!< Write latency
    uint32_t tDQSCK;  //!< DQS Output Access Time from CK
    uint32_t tWR;     //!< Write recovery
    uint32_t tWTR;    //!< Write to Read latency
    uint32_t tRTP;    //!< Read to Precharge
    uint32_t tRFC;    //!< Refresh time (Per bank)
    uint32_t tRFCab;  //!< Refresh time (All banks)
    uint32_t tREFI;   //!< Refresh command interval
    uint32_t tSR;     //!< Self refresh exit latency
    uint32_t tXSV;    //!< Exit self refresh to valid commands
    uint32_t tFAW;    //!< Bank activation limit window
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

  //! DRAM Controller parameters.
  struct DRAMController {
    uint16_t readQueueSize;
    uint16_t writeQueueSize;
    float writeMinThreshold;
    float writeMaxThreshold;
    uint32_t minWriteBurst;
    MemoryScheduling schedulePolicy;
    AddressMapping addressPolicy;
    PagePolicy pagePolicy;
  };

 private:
  uint64_t systemBusSpeed;

  CacheConfig llc;
  SRAMStructure sram;
  DRAMStructure dram;
  DRAMTiming timing;
  DRAMPower power;
  DRAMController controller;

  Model dramModel;

  void loadCache(pugi::xml_node &, CacheConfig &) noexcept;
  void loadSRAM(pugi::xml_node &) noexcept;
  void loadDRAMStructure(pugi::xml_node &) noexcept;
  void loadDRAMTiming(pugi::xml_node &) noexcept;
  void loadDRAMPower(pugi::xml_node &) noexcept;
  void loadTimingDRAM(pugi::xml_node &) noexcept;
  void storeCache(pugi::xml_node &, CacheConfig &) noexcept;
  void storeSRAM(pugi::xml_node &) noexcept;
  void storeDRAMStructure(pugi::xml_node &) noexcept;
  void storeDRAMTiming(pugi::xml_node &) noexcept;
  void storeDRAMPower(pugi::xml_node &) noexcept;
  void storeTimingDRAM(pugi::xml_node &) noexcept;

 public:
  Config();

  const char *getSectionName() noexcept override { return "memory"; }

  void loadFrom(pugi::xml_node &) noexcept override;
  void storeTo(pugi::xml_node &) noexcept override;

  uint64_t readUint(uint32_t) const noexcept override;
  bool writeUint(uint32_t, uint64_t) noexcept override;

  CacheConfig *getLLC() noexcept;
  SRAMStructure *getSRAM() noexcept;
  DRAMStructure *getDRAM() noexcept;
  DRAMTiming *getDRAMTiming() noexcept;
  DRAMPower *getDRAMPower() noexcept;
  DRAMController *getDRAMController() noexcept;
};

}  // namespace SimpleSSD::Memory

#endif
