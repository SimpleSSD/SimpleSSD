// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_SIM_CONFIG_HH__
#define __SIMPLESSD_SIM_CONFIG_HH__

#include "sim/base_config.hh"

#define FILE_STDOUT "STDOUT"
#define FILE_STDERR "STDERR"

namespace SimpleSSD {

/**
 * \brief SimConfig object declaration
 *
 * Stores simulation configurations such as simulation output directory.
 */
class Config : public BaseConfig {
 public:
  enum Key : uint32_t {
    OutputDirectory,
    OutputFile,
    ErrorFile,
    DebugFile,
    Controller,
    RestoreFromCheckpoint,
  };

  enum class Mode : uint8_t {
    None,
    NVMe,
    SATA,
    UFS,
  };

 private:
  std::string outputDirectory;
  std::string outputFile;
  std::string errorFile;
  std::string debugFile;
  Mode mode;
  bool restore;

 public:
  Config();

  const char *getSectionName() noexcept override { return "sim"; }

  void loadFrom(pugi::xml_node &) noexcept override;
  void storeTo(pugi::xml_node &) noexcept override;

  uint64_t readUint(uint32_t) const noexcept override;
  std::string readString(uint32_t) const noexcept override;
  bool readBoolean(uint32_t) const noexcept override;
  bool writeUint(uint32_t, uint64_t) noexcept override;
  bool writeString(uint32_t, std::string &) noexcept override;
  bool writeBoolean(uint32_t, bool) noexcept override;
};

}  // namespace SimpleSSD

#endif
