// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "sim/config.hh"

namespace SimpleSSD {

//! Config constructor
Config::Config() {}

//! Config destructor
Config::~Config() {}

/**
 * \brief Load configuration from file
 *
 * \param[in] path Input file path
 */
void Config::load(const char *path) {
  auto result = file.load_file(path, pugi::parse_default, pugi::encoding_utf8);

  // panic_if(!result, "Failed to parse configuration file: %s",
  //          result.description());
}

//! Load configuration from file
void Config::load(std::string &path) {
  load(path.c_str());
}

/**
 * \brief Save configuration to file
 *
 * \param[in] path Output file path
 */
void Config::save(const char *path) {
  auto result =
      file.save_file(path, "  ", pugi::format_default, pugi::encoding_utf8);

  // panic_if(!result, "Failed to save configuration file: %s",
  //          result.description());
}

//! Save configuration to file
void Config::save(std::string &path) {
  save(path.c_str());
}

}  // namespace SimpleSSD
