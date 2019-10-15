// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_MEM_DRAM_SIMPLE_HH__
#define __SIMPLESSD_MEM_DRAM_SIMPLE_HH__

#include "mem/dram/abstract_dram.hh"
#include "util/scheduler.hh"

namespace SimpleSSD::Memory::DRAM {

class SimpleDRAM : public AbstractDRAM {
 private:
  Scheduler<Request *> scheduler;

  std::vector<std::pair<uint64_t, uint64_t>> addressMap;

  uint64_t pageFetchLatency;
  uint64_t unallocated;
  double interfaceBandwidth;

  Event autoRefresh;

  uint64_t preSubmitRead(Request *);
  uint64_t preSubmitWrite(Request *);
  void postDone(Request *);

 public:
  SimpleDRAM(ObjectData &);
  ~SimpleDRAM();

  uint64_t allocate(uint64_t) override;

  void read(uint64_t, uint64_t, Event) override;
  void write(uint64_t, uint64_t, Event) override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::Memory::DRAM

#endif
