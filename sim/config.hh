// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIM_CONFIG_HH__
#define __SIM_CONFIG_HH__

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
  };

 private:
  std::string outputDirectory;
  std::string outputFile;
  std::string errorFile;
  std::string debugFile;

 public:
  Config();
  ~Config();

  const char *getSectionName() override { return "sim"; }

  void loadFrom(pugi::xml_node &) override;
  void storeTo(pugi::xml_node &) override;

  std::string readString(uint32_t) override;
  bool writeString(uint32_t, std::string &) override;
};

}  // namespace SimpleSSD

#endif
