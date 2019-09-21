// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIM_CONFIG_HH__
#define __SIM_CONFIG_HH__

#include <string>

#include "cpu/config.hh"
#include "mem/config.hh"
#include "sim/sim_config.hh"

namespace SimpleSSD {

/**
 * \brief Config object declaration
 *
 * SSD configuration object. This object provides configuration parser.
 * Also, you can override configuration by calling set function.
 */
class Config {
 public:
  enum class Section {
    Simulation,
    CPU,
    Memory,
  };

 private:
  pugi::xml_document file;

  SimConfig simConfig;
  CPU::Config cpuConfig;
  Memory::Config memConfig;

 public:
  Config();
  Config(const Config &) = delete;
  Config(Config &&) = default;
  ~Config();

  Config &operator=(const Config &) = delete;
  Config &operator=(Config &&) noexcept = default;

  void load(const char *) noexcept;
  void load(std::string &) noexcept;

  void save(const char *) noexcept;
  void save(std::string &) noexcept;

  int64_t readInt(Section, uint32_t);
  uint64_t readUint(Section, uint32_t);
  float readFloat(Section, uint32_t);
  std::string readString(Section, uint32_t);
  bool readBoolean(Section, uint32_t);

  bool writeInt(Section, uint32_t, int64_t);
  bool writeUint(Section, uint32_t, uint64_t);
  bool writeFloat(Section, uint32_t, float);
  bool writeString(Section, uint32_t, std::string);
  bool writeBoolean(Section, uint32_t, bool);
};

}  // namespace SimpleSSD

#endif
