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
#include "mem/abstract_ram.hh"

namespace SimpleSSD::Memory::DRAM {

class AbstractDRAM : public AbstractRAM {
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

  Config::DRAMStructure *pStructure;
  Config::DRAMTiming *pTiming;
  Config::DRAMPower *pPower;

  Data::MemorySpecification spec;
  libDRAMPower *dramPower;

  Stats readStat;
  Stats writeStat;
  double totalEnergy;  // Unit: pJ
  double totalPower;   // Unit: mW

  void convertMemspec();
  void updateStats(uint64_t);

 public:
  AbstractDRAM(ObjectData &);
  virtual ~AbstractDRAM();

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::Memory::DRAM

#endif
