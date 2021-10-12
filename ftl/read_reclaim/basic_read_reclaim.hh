// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_FTL_READ_RECLAIM_BASIC_READ_RECLAIM_HH__
#define __SIMPLESSD_FTL_READ_RECLAIM_BASIC_READ_RECLAIM_HH__

#include "ftl/read_reclaim/abstract_read_reclaim.hh"
#include "util/sorted_map.hh"

namespace SimpleSSD::FTL::ReadReclaim {

class BasicReadReclaim : public AbstractReadReclaim {
 protected:
  uint64_t beginAt;

  map_list<uint64_t, PSBN> pendingList;

  struct {
    uint64_t foreground;
    uint64_t background;
    uint64_t copiedPages;
    uint64_t erasedBlocks;
  } stat;

  void readPage(uint64_t, uint32_t) override;
  void done(uint64_t, uint32_t) override;

 public:
  BasicReadReclaim(ObjectData &, FTLObjectData &, FIL::FIL *);
  ~BasicReadReclaim();

  void initialize() override;

  bool doErrorCheck(const PPN &) override;

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::FTL::ReadReclaim

#endif
