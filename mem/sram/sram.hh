// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_MEM_SRAM_SRAM_HH__
#define __SIMPLESSD_MEM_SRAM_SRAM_HH__

#include "mem/sram/abstract_sram.hh"
#include "util/scheduler.hh"

namespace SimpleSSD::Memory::SRAM {

class SRAM : public AbstractSRAM {
 protected:
  Scheduler<Request *> scheduler;

  uint64_t preSubmit(Request *);
  void postDone(Request *);

 public:
  SRAM(ObjectData &);
  ~SRAM();

  void read(uint64_t, uint64_t, Event, uint64_t) override;
  void write(uint64_t, uint64_t, Event, uint64_t) override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::Memory::SRAM

#endif
