// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIM_SIM_CONFIG_HH__
#define __SIM_SIM_CONFIG_HH__

#include "sim/base_config.hh"

namespace SimpleSSD {

/**
 * \brief SimConfig object declaration
 *
 * Stores simulation configurations such as simulation output directory.
 */
class SimConfig : public BaseConfig {
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
  SimConfig();
  ~SimConfig();

  const char *getSectionName() { return "sim"; }

  void loadFrom(pugi::xml_node &);
  void storeTo(pugi::xml_node &);

  std::string readString(uint32_t);
  bool writeString(uint32_t, std::string);
};

}  // namespace SimpleSSD

#endif
