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
  libDRAMPower *dramPower;

  Config::DRAMStructure *pStructure;
  Config::DRAMTiming *pTiming;
  Config::DRAMPower *pPower;

  Stats readStat;
  Stats writeStat;

 public:
  AbstractDRAM(ObjectData &);
  ~AbstractDRAM();

  virtual void read(uint64_t address, uint32_t length, Event eid,
                    uint64_t data = 0) = 0;
  virtual void write(uint64_t address, uint32_t length, Event eid,
                     uint64_t data = 0) = 0;

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::Memory::DRAM

#endif
