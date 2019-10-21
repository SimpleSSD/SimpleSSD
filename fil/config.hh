// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_FIL_CONFIG_HH__
#define __SIMPLESSD_FIL_CONFIG_HH__

#include "fil/def.hh"
#include "sim/base_config.hh"

#define NAND_MAX_LEVEL 3  // TLC

namespace SimpleSSD::FIL {

class Config : public BaseConfig {
 public:
  enum Key : uint32_t {
    Channel,
    Way,
    DMASpeed,
    DMAWidth,
    Model,
  };

  enum class NVMType : uint8_t {
    PAL,
    GenericNAND,
  };

  enum class NANDType : uint8_t {
    SLC,
    MLC,
    TLC,
  };

  struct NANDStructure {
    NANDType type;
    uint8_t nop;
    PageAllocation pageAllocation[4];
    uint32_t die;
    uint32_t plane;
    uint32_t block;
    uint32_t page;
    uint32_t pageSize;
    uint32_t spareSize;
  };

  struct NANDTiming {
    uint32_t tADL;  //!< Address cycle to Data Load time
    uint32_t tCS;   //!< CE_n Setup time
    uint32_t tDH;   //!< Data Hold time
    uint32_t tDS;   //!< Data Setup time
    uint32_t tRC;   //!< RE_n Cycle time
    uint32_t tRR;   //!< Ready to data output cycle
    uint32_t tWB;   //!< WE_n high to SR[6] low
    uint32_t tWC;   //!< WE_n Cycle time
    uint32_t tWP;   //!< WE_n Pulse width

    uint32_t tCBSY;   //!< Cache busy time
    uint32_t tDBSY;   //!< Dummy busy time (tPLRBSY/tPLPBSY/tPLEBSY)
    uint32_t tRCBSY;  //!< Read Cache busy time

    uint64_t tBERS;                  //!< Block erase time
    uint64_t tPROG[NAND_MAX_LEVEL];  //!< Program time
    uint64_t tR[NAND_MAX_LEVEL];     //!< Read time
  };

  struct NANDPower {
    uint64_t pVCC;      //!< Unit: mV
    struct {            //!< Unit: uA
      uint64_t pICC1;   //!< Array Read Current
      uint64_t pICC2;   //!< Array Program Current
      uint64_t pICC3;   //!< Array Erase Current
      uint64_t pICC4R;  //!< I/O Burst Read Current
      uint64_t pICC4W;  //!< I/O Burst Write Current
      uint64_t pICC5;   //!< Bus Idle Current
      uint64_t pISB;    //!< Standby Current
    } current;
  };

 private:
  uint32_t channel;   //!< Default: 8
  uint32_t package;   //!< Default: 4
  uint32_t dmaSpeed;  //!< Default: 400
  uint32_t dmaWidth;  //!< Default: 8
  NVMType nvmModel;   //!< Default: NVMType::PAL

  NANDStructure nandStructure;
  NANDTiming nandTiming;
  NANDPower nandPower;

  std::string _pageAllocation;

  void loadNANDStructure(pugi::xml_node &);
  void loadNANDTiming(pugi::xml_node &);
  void loadNANDPower(pugi::xml_node &);
  void storeNANDStructure(pugi::xml_node &);
  void storeNANDTiming(pugi::xml_node &);
  void storeNANDPower(pugi::xml_node &);

 public:
  Config();
  const char *getSectionName() override { return "fil"; }

  void loadFrom(pugi::xml_node &) override;
  void storeTo(pugi::xml_node &) override;
  void update() override;

  uint64_t readUint(uint32_t) override;
  bool writeUint(uint32_t, uint64_t) override;

  NANDStructure *getNANDStructure();
  NANDTiming *getNANDTiming();
  NANDPower *getNANDPower();
};

}  // namespace SimpleSSD::FIL

#endif
