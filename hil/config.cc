// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "hil/config.hh"

namespace SimpleSSD::HIL {

const char NAME_CLOCK[] = "ClockSpeed";
const char NAME_CORE_HIL[] = "HILCoreCount";
const char NAME_CORE_ICL[] = "ICLCoreCount";
const char NAME_CORE_FTL[] = "FTLCoreCount";

//! A constructor
Config::Config() {
  clock = 400000000;
  hilCore = 1;
  iclCore = 1;
  ftlCore = 1;
}

//! A destructor
Config::~Config() {}

void Config::loadFrom(pugi::xml_node &section) {
  for (auto node = section.first_child(); node; node = node.next_sibling()) {
    LOAD_NAME_UINT(node, NAME_CLOCK, clock);
    LOAD_NAME_UINT_TYPE(node, NAME_CORE_HIL, uint32_t, hilCore);
    LOAD_NAME_UINT_TYPE(node, NAME_CORE_ICL, uint32_t, iclCore);
    LOAD_NAME_UINT_TYPE(node, NAME_CORE_FTL, uint32_t, ftlCore);
  }
}

void Config::storeTo(pugi::xml_node &section) {
  STORE_NAME_UINT(section, NAME_CLOCK, clock);
  STORE_NAME_UINT(section, NAME_CORE_HIL, hilCore);
  STORE_NAME_UINT(section, NAME_CORE_ICL, iclCore);
  STORE_NAME_UINT(section, NAME_CORE_FTL, ftlCore);
}

void Config::update() {
  panic_if(clock == 0, "Invalid clock speed");
}

uint64_t Config::readUint(uint32_t idx) {
  uint64_t ret = 0;

  switch (idx) {
    case Key::Clock:
      ret = clock;
      break;
    case Key::HILCore:
      ret = hilCore;
      break;
    case Key::ICLCore:
      ret = iclCore;
      break;
    case Key::FTLCore:
      ret = ftlCore;
      break;
  }

  return ret;
}

bool Config::writeUint(uint32_t idx, uint64_t value) {
  bool ret = true;

  switch (idx) {
    case Key::Clock:
      clock = value;
      break;
    case Key::HILCore:
      hilCore = (uint32_t)value;
      break;
    case Key::ICLCore:
      iclCore = (uint32_t)value;
      break;
    case Key::FTLCore:
      ftlCore = (uint32_t)value;
      break;
    default:
      ret = false;
      break;
  }

  return ret;
}

}  // namespace SimpleSSD::HIL
