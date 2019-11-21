// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_MEM_DRAM_ABSTRACT_DRAM_HH__
#define __SIMPLESSD_MEM_DRAM_ABSTRACT_DRAM_HH__

#include <cinttypes>

#include "libdrampower/LibDRAMPower.h"
#include "sim/object.hh"

namespace SimpleSSD::Memory::DRAM {

class AbstractDRAM : public Object {
 protected:
  struct Stats {
    uint64_t count;
    uint64_t size;

    Stats() { clear(); }

    void clear() {
      count = 0;
      size = 0;
    }
  };

  Data::MemorySpecification spec;
  std::vector<libDRAMPower> dramPower;

  Config::DRAMStructure *pStructure;
  Config::DRAMTiming *pTiming;
  Config::DRAMPower *pPower;

  // Statistics
  Stats readStat;
  Stats writeStat;

  struct PowerStat {
    double act_energy;
    double pre_energy;
    double read_energy;
    double write_energy;
    double ref_energy;
    double sref_energy;
    double window_energy;

    PowerStat() { clear(); }

    void clear() {
      act_energy = 0.;
      pre_energy = 0.;
      read_energy = 0.;
      write_energy = 0.;
      ref_energy = 0.;
      sref_energy = 0.;
      window_energy = 0.;
    }
  };

  std::vector<PowerStat> powerStat;

  uint64_t lastResetAt;

 public:
  AbstractDRAM(ObjectData &);
  ~AbstractDRAM();

  virtual bool isIdle(uint32_t, uint8_t) = 0;
  virtual uint32_t getRowInfo(uint32_t, uint8_t) = 0;
  virtual void submit(Address, uint32_t, bool, Event, uint64_t) = 0;

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::Memory::DRAM

#endif
