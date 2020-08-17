// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_MEM_DRAM_IDEAL_IDEAL_HH__
#define __SIMPLESSD_MEM_DRAM_IDEAL_IDEAL_HH__

#include "libdrampower/LibDRAMPower.h"
#include "mem/def.hh"
#include "mem/dram/abstract_dram.hh"
#include "util/scheduler.hh"

namespace SimpleSSD::Memory::DRAM {

/**
 * \brief Ideal DRAM model
 *
 * This model only calculates DRAM bus latency.
 */
class IdealDRAM : public AbstractDRAM {
 private:
  SingleScheduler<Request *> scheduler;

  double packetLatency;

  Data::MemorySpecification spec;
  libDRAMPower *dramPower;

  void convertMemspec();

  double totalEnergy;  // Unit: pJ
  double totalPower;   // Unit: mW

  void updateStats(uint64_t cycle) noexcept;

  uint64_t preSubmit(Request *);
  void postDone(Request *);

 public:
  IdealDRAM(ObjectData &);
  ~IdealDRAM();

  void read(uint64_t, Event, uint64_t = 0) override;
  void write(uint64_t, Event, uint64_t = 0) override;

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::Memory::DRAM

#endif
