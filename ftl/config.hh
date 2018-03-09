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
  FTL_BAD_BLOCK_THRESHOLD,
  FTL_WARM_UP_RATIO,
  FTL_GC_MODE,
  FTL_GC_RECLAIM_BLOCK,
  FTL_GC_RECLAIM_THRESHOLD,
  FTL_GC_EVICT_POLICY,
  FTL_LATENCY,
  FTL_REQUEST_QUEUE,

  /* N+K Mapping configuration*/
  FTL_NKMAP_N,
  FTL_NKMAP_K,
} FTL_CONFIG;

typedef enum {
  PAGE_MAPPING,
} MAPPING;

typedef enum {
  GC_MODE_0,
  GC_MODE_1,
} GC_MODE;

typedef enum {
  POLICY_GREEDY,
  POLICY_COST_BENEFIT,
} EVICT_POLICY;

class Config : public BaseConfig {
 private:
  MAPPING mapping;             //!< Default: PAGE_MAPPING
  float overProvision;         //!< Default: 0.25 (25%)
  float gcThreshold;           //!< Default: 0.05 (5%)
  uint64_t badBlockThreshold;  //!< Default: 100000
  float warmup;                //!< Default: 1.0 (100%)
  uint64_t reclaimBlock;       //!< Default: 1
  float reclaimThreshold;      //!< Default: 0.1 (10%)
  GC_MODE gcMode;              //!< Default: FTL_GC_MODE_0
  EVICT_POLICY evictPolicy;    //!< Default: POLICY_GREEDY
  uint64_t latency;            //!< Default: 50us
  uint64_t requestQueue;       //!< Default: 1

 public:
  Config();

  bool setConfig(const char *, const char *) override;
  void update() override;

  int64_t readInt(uint32_t) override;
  uint64_t readUint(uint32_t) override;
  float readFloat(uint32_t) override;
};

}  // namespace FTL

}  // namespace SimpleSSD

#endif
