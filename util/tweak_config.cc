/*
 * Copyright (C) 2017 CAMELab
 *
 * This file is part of SimpleSSD.
 *
 * SimpleSSD is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * SimpleSSD is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with SimpleSSD.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "util/tweak_config.hh"

namespace SimpleSSD {

const char NAME_PARTIAO_IO[] = "EnablePartialIO";

TweakConfig::TweakConfig() {
  enablePartialIO = false;
}

bool TweakConfig::setConfig(const char *name, const char *value) {
  bool ret = true;

  if (MATCH_NAME(NAME_PARTIAO_IO)) {
    enablePartialIO = convertBool(value);
  }
  else {
    ret = false;
  }

  return ret;
}

void TweakConfig::update() {}

int64_t TweakConfig::readInt(uint32_t idx) {
  int64_t ret = 0;

  return ret;
}

uint64_t TweakConfig::readUint(uint32_t idx) {
  uint64_t ret = 0;

  return ret;
}

float TweakConfig::readFloat(uint32_t idx) {
  float ret = 0.f;

  return ret;
}

std::string TweakConfig::readString(uint32_t idx) {
  std::string ret("");

  return ret;
}

bool TweakConfig::readBoolean(uint32_t idx) {
  bool ret = false;

  switch (idx) {
    case TWEAK_PARTIAL_IO:
      ret = enablePartialIO;
      break;
  }

  return ret;
}

}  // namespace SimpleSSD
