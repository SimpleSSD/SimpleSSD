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

#ifndef __UTIL_TWEAK_CONFIG__
#define __UTIL_TWEAK_CONFIG__

#include "util/base_config.hh"

namespace SimpleSSD {

typedef enum { TWEAK_PARTIAL_IO } TWEAK_CONFIG;

class TweakConfig : public BaseConfig {
 private:
  bool enablePartialIO;  //!< Default: false

 public:
  TweakConfig();

  bool setConfig(const char *, const char *);
  void update();

  int64_t readInt(uint32_t);
  uint64_t readUint(uint32_t);
  float readFloat(uint32_t);
  std::string readString(uint32_t);
  bool readBoolean(uint32_t);
};

}  // namespace SimpleSSD

#endif
