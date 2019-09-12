// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __MEM_MEM_CONFIG_HH__
#define __MEM_MEM_CONFIG_HH__

#include "sim/base_config.hh"

namespace SimpleSSD {

/**
 * \brief MemConfig object declaration
 *
 * Stores DRAM and cache configurations.
 */
class MemConfig : public BaseConfig {
 public:
  enum Key : uint32_t {
    Level1Cache,
    Level2Cache,
    DRAMStructure,
    DRAMTiming,
    DRAMPower,
  };

  enum Model : uint8_t {
    Simple,
    SetAssociative,
    Full = 1,
  };

  struct CacheParameter {
    uint8_t model;
    uint8_t way;
    uint16_t lineSize;
    uint32_t set;
    uint64_t size;
    uint64_t latency;
  };

  struct DRAMParameter {
    uint8_t channel;
    uint8_t rank;
    uint8_t bank;
    uint8_t chip;
    uint16_t width;
    uint16_t burst;
    uint64_t chipsize;
  };

  struct DRAMTimingParameter {

  };

  struct DRAMPowerParameter {

  };

 private:
  CacheParameter level1;
  CacheParameter level2;
  DRAMParameter dram;
  DRAMTimingParameter timing;
  DRAMPowerParameter power;

  uint8_t dramModel;

  void loadCache(pugi::xml_node &, CacheParameter *);
  void loadDRAMStructure(pugi::xml_node &, DRAMParameter *);
  void loadDRAMTiming(pugi::xml_node &, DRAMTimingParameter *);
  void loadDRAMPower(pugi::xml_node &, DRAMPowerParameter *);
  void storeCache(pugi::xml_node &, CacheParameter *);
  void storeDRAMStructure(pugi::xml_node &, DRAMParameter *);
  void storeDRAMTiming(pugi::xml_node &, DRAMTimingParameter *);
  void storeDRAMPower(pugi::xml_node &, DRAMPowerParameter *);

 public:
  MemConfig();
  ~MemConfig();

  const char *getSectionName() override { return "memory"; }

  void loadFrom(pugi::xml_node &) override;
  void storeTo(pugi::xml_node &) override;
  void update() override;
};

}  // namespace SimpleSSD

#endif
