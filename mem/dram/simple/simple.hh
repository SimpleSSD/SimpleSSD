// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_MEM_DRAM_SIMPLE_SIMPLE_HH__
#define __SIMPLESSD_MEM_DRAM_SIMPLE_SIMPLE_HH__

#include <map>

#include "mem/dram/abstract_dram.hh"

namespace SimpleSSD::Memory::DRAM {

/**
 * \brief Simple DRAM model
 *
 * Very simple DRAM model with Rank/Bank/Row/Col calculation.
 * No scheduling - FCFS - and no refresh - self/auto refresh.
 */
class Simple : public AbstractDRAM {
 private:

 public:
  Simple(ObjectData &);
  ~Simple();

  void read(uint64_t, Event, uint64_t = 0);
  void write(uint64_t, Event, uint64_t = 0);

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::Memory::DRAM

#endif
