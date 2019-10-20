// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_FTL_DEF_HH__
#define __SIMPLESSD_FTL_DEF_HH__

#include <cinttypes>
#include <unordered_map>

#include "sim/event.hh"
#include "util/bitset.hh"

namespace SimpleSSD::FTL {

typedef struct {
  uint64_t totalPhysicalPages;
  uint64_t totalLogicalPages;
  uint32_t pageSize;
  uint32_t parallelismLevel[4];  //!< Parallelism group list
  uint8_t superpageLevel;  //!< Number of levels (1~N) included in superpage
} Parameter;

struct BlockInfo {
  PPN blockID;

  Bitset valid;
  std::vector<std::pair<LPN, PPN>> lpnList;

  BlockInfo(uint32_t s) : valid(s), lpnList(s, std::make_pair(0, 0)) {}
};

}  // namespace SimpleSSD::FTL

#endif
