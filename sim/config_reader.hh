// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_SIM_CONFIG_READER_HH__
#define __SIMPLESSD_SIM_CONFIG_READER_HH__

#include <string>

#include "cpu/config.hh"
#include "fil/config.hh"
#include "ftl/config.hh"
#include "hil/config.hh"
#include "icl/config.hh"
#include "mem/config.hh"
#include "sim/config.hh"

namespace SimpleSSD {

//! Configuration section enum.
enum class Section {
  Simulation,
  CPU,
  Memory,
  HostInterface,
  InternalCache,
  FlashTranslation,
  FlashInterface,
};

/**
 * \brief ConfigReader object declaration
 *
 * SSD configuration object. This object provides configuration parser.
 * Also, you can override configuration by calling set function.
 */
class ConfigReader {
 private:
  pugi::xml_document file;

  Config simConfig;
  CPU::Config cpuConfig;
  Memory::Config memConfig;
  HIL::Config hilConfig;
  ICL::Config iclConfig;
  FTL::Config ftlConfig;
  FIL::Config filConfig;

 public:
  ConfigReader();
  ConfigReader(const ConfigReader &) = delete;
  ConfigReader(ConfigReader &&) noexcept = default;
  ~ConfigReader();

  ConfigReader &operator=(const ConfigReader &) = delete;
  ConfigReader &operator=(ConfigReader &&) = default;

  void load(const char *, bool = false) noexcept;
  void load(std::string &, bool = false) noexcept;

  void save(const char *) noexcept;
  void save(std::string &) noexcept;

  int64_t readInt(Section, uint32_t) const noexcept;
  uint64_t readUint(Section, uint32_t) const noexcept;
  float readFloat(Section, uint32_t) const noexcept;
  std::string readString(Section, uint32_t) const noexcept;
  bool readBoolean(Section, uint32_t) const noexcept;

  bool writeInt(Section, uint32_t, int64_t) noexcept;
  bool writeUint(Section, uint32_t, uint64_t) noexcept;
  bool writeFloat(Section, uint32_t, float) noexcept;
  bool writeString(Section, uint32_t, std::string) noexcept;
  bool writeBoolean(Section, uint32_t, bool) noexcept;

  // Interface for Memory::Config
  Memory::Config::SRAMStructure *getSRAM() noexcept;
  Memory::Config::DRAMStructure *getDRAM() noexcept;
  Memory::Config::DRAMTiming *getDRAMTiming() noexcept;
  Memory::Config::DRAMPower *getDRAMPower() noexcept;
  Memory::Config::DRAMController *getDRAMController() noexcept;

  // Interface for HIL::Config
  std::vector<HIL::Config::Disk> &getDiskList() noexcept;
  std::vector<HIL::Config::Namespace> &getNamespaceList() noexcept;

  // Interface for FIL::Config
  FIL::Config::NANDStructure *getNANDStructure() noexcept;
  FIL::Config::NANDTiming *getNANDTiming() noexcept;
  FIL::Config::NANDPower *getNANDPower() noexcept;
};

}  // namespace SimpleSSD

#endif
