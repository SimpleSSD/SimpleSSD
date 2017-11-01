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

#ifndef __FTL_CONFIG__
#define __FTL_CONFIG__

#include "util/base_config.hh"

namespace SimpleSSD {

namespace FTL {

typedef enum {
  /* Common FTL configuration */
  FTL_MAPPING_MODE,
  FTL_OVERPROVISION_RATIO,
  FTL_GC_THRESHOLD_RATIO,
  FTL_ERASE_CYCLE,
  FTL_WARM_UP_RATIO,

  /* N+K Mapping configuration*/
  FTL_NKMAP_N,
  FTL_NKMAP_K,
} FTL_CONFIG;

typedef enum {
  FTL_PAGE_MAPPING,
  FTL_NK_MAPPING,
} FTL_MAPPING;

class Config : public BaseConfig {
 private:
  FTL_MAPPING mapping;  //!< Default: FTL_NK_MAPPING
  float overProvision;  //!< Default: 0.25 (25%)
  float gcThreshold;    //!< Default: 0.05 (5%)
  uint64_t eraseCycle;  //!< Default: 100000
  float warmup;         //!< Default: 1.0 (100%)

  uint64_t N;  //!< Default: 32
  uint64_t K;  //!< Default: 32

 public:
  Config();

  bool setConfig(const char *, const char *);
  void update();

  int64_t readInt(uint32_t);
  uint64_t readUint(uint32_t);
  float readFloat(uint32_t);
  std::string readString(uint32_t);
  bool readBoolean(uint32_t);
};

}  // namespace FTL

}  // namespace SimpleSSD

#endif
