// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIM_BASE_CONFIG_HH__
#define __SIM_BASE_CONFIG_HH__

#include <cinttypes>
#include <string>

#include "lib/pugixml/src/pugixml.hpp"

namespace SimpleSSD {

/**
 * \brief BaseConfig object declaration
 *
 * Abstract class for configuration object. You must inherit this class to
 * create your own configuration section in xml file.
 */
class BaseConfig {
 public:
  BaseConfig() {}
  virtual ~BaseConfig() {}

  virtual bool setConfig(pugi::xml_node &) = 0;
  virtual void update() {}

  virtual int64_t readInt(uint32_t) { return 0; }
  virtual uint64_t readUint(uint32_t) { return 0; }
  virtual float readFloat(uint32_t) { return 0.f; }
  virtual std::string readString(uint32_t) { return std::string(); }
  virtual bool readBoolean(uint32_t) { return false; }
};

}  // namespace SimpleSSD

#endif
