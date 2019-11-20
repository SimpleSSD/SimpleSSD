// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_MEM_DRAM_IDEAL_HH__
#define __SIMPLESSD_MEM_DRAM_IDEAL_HH__

#include "mem/dram/abstract_dram.hh"
#include "util/scheduler.hh"

namespace SimpleSSD::Memory::DRAM {

class Ideal : public AbstractDRAM {
 private:
  Scheduler<Request *> scheduler;

  double interfaceBandwidth;
  uint64_t pageSize;
  uint64_t bankSize;

  uint64_t preSubmit(Request *);
  void postDone(Request *);

 public:
  Ideal(ObjectData &);
  ~Ideal();

  bool isNotRefresh(uint32_t, uint8_t) override;
  uint32_t getRowInfo(uint32_t, uint8_t) override;
  void submit(Address, uint32_t, bool, Event, uint64_t) override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::Memory::DRAM

#endif
