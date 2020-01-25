// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_MEM_DRAM_SIMPLE_SIMPLE_HH__
#define __SIMPLESSD_MEM_DRAM_SIMPLE_SIMPLE_HH__

#include "mem/dram/abstract_dram.hh"

namespace SimpleSSD::Memory::DRAM::Simple {

/**
 * \brief Simple DRAM model
 *
 * Simple DRAM model with Rank/Bank/Row/Col calculation.
 * Inspired by DRAMSim2.
 */
class SimpleDRAM : public AbstractDRAM {
 private:
 public:
  SimpleDRAM(ObjectData &);
  ~SimpleDRAM();

  void read(uint64_t, Event, uint64_t = 0);
  void write(uint64_t, Event, uint64_t = 0);

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::Memory::DRAM::Simple

#endif
