// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_MEM_DRAM_SIMPLE_RANK_HH__
#define __SIMPLESSD_MEM_DRAM_SIMPLE_RANK_HH__

#include <deque>

#include "mem/dram/simple/bank.hh"

namespace SimpleSSD::Memory::DRAM::Simple {

class Controller;

/**
 * \brief Simple Rank model
 *
 * Simple DRAM Rank with bank state machine.
 */
class Rank : public Object {
 private:
  Controller *parent;
  Timing *timing;

  bool pendingRefresh;

  std::vector<Bank> banks;

  CountStat readStat;
  CountStat writeStat;

 public:
  Rank(ObjectData &, Controller *, Timing *);
  ~Rank();

  bool submit(Packet *);

  void powerEvent();
  void completion(uint64_t);

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::Memory::DRAM::Simple

#endif
