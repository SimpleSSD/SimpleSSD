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

#include "lib/pugixml/src/pugixml.hpp"

namespace SimpleSSD {

/**
 * \brief Config object declaration
 *
 * SSD configuration object. This object provides configuration parser.
 * Also, you can override configuration by calling set function.
 */
class Config {
 private:
  pugi::xml_document file;

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
};

}  // namespace SimpleSSD

#endif
