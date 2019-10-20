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

struct CopyList {
  PPN blockID;

  std::vector<uint64_t>::iterator iter;
  std::vector<uint64_t> commandList;

  void resetIterator() { iter = commandList.begin(); }

  bool isEnd() { return iter == commandList.end(); }

  void createCheckpoint(std::ostream &out) const {
    BACKUP_SCALAR(out, blockID);

    uint64_t size = commandList.size();
    BACKUP_SCALAR(out, size);

    for (auto &iter : commandList) {
      BACKUP_SCALAR(out, iter);
    }

    size = iter - commandList.begin();
    BACKUP_SCALAR(out, size);
  }

  void restoreCheckpoint(std::istream &in) {
    RESTORE_SCALAR(in, blockID);

    uint64_t size;
    RESTORE_SCALAR(in, size);

    for (uint64_t i = 0; i < size; i++) {
      uint64_t tag;

      RESTORE_SCALAR(in, tag);

      commandList.emplace_back(tag);
    }

    RESTORE_SCALAR(in, size);
    iter = commandList.begin() + size;
  }
};

}  // namespace SimpleSSD::FTL

#endif
