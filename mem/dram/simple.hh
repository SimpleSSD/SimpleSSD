// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_MEM_DRAM_SIMPLE__
#define __SIMPLESSD_MEM_DRAM_SIMPLE__

#include "mem/dram/abstract_dram.hh"

namespace SimpleSSD::Memory::DRAM {

class SimpleDRAM : public AbstractDRAM {
 private:
  uint64_t pageFetchLatency;
  double interfaceBandwidth;

  Event autoRefresh;

 public:
  SimpleDRAM(ObjectData &);
  ~SimpleDRAM();

  void read(uint64_t, uint64_t, Event) override;
  void write(uint64_t, uint64_t, Event) override;

  void createCheckpoint(std::ostream &) noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::Memory::DRAM

#endif
