// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_MEM_DRAM_IDEAL_IDEAL_HH__
#define __SIMPLESSD_MEM_DRAM_IDEAL_IDEAL_HH__

#include "mem/def.hh"
#include "mem/dram/abstract_dram.hh"
#include "util/scheduler.hh"

namespace SimpleSSD::Memory::DRAM::Ideal {

/**
 * \brief Ideal DRAM model
 *
 * This model only calculates DRAM bus latency.
 */
class IdealDRAM : public AbstractDRAM {
 private:
  Scheduler<Request *> scheduler;

  double packetLatency;

  uint64_t preSubmit(Request *);
  void postDone(Request *);

 public:
  IdealDRAM(ObjectData &);
  ~IdealDRAM();

  void read(uint64_t, Event, uint64_t = 0) override;
  void write(uint64_t, Event, uint64_t = 0) override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::Memory::DRAM::Ideal

#endif
