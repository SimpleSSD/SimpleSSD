// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __MEM_DRAM_SIMPLE__
#define __MEM_DRAM_SIMPLE__

#include "mem/dram/abstract_dram.hh"

namespace SimpleSSD::Memory::DRAM {

class SimpleDRAM : public AbstractDRAM {
 private:
  uint64_t pageFetchLatency;
  double interfaceBandwidth;

  Event autoRefresh;

 public:
  SimpleDRAM(ObjectData &&);
  ~SimpleDRAM();

  void read(uint64_t, uint64_t, Event, void * = nullptr) override;
  void write(uint64_t, uint64_t, Event, void * = nullptr) override;

  void createCheckpoint() noexcept override;
  void restoreCheckpoint() noexcept override;
};

}  // namespace SimpleSSD::Memory::DRAM

#endif
