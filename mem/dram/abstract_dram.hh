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

union Address {
  uint64_t data;
  struct {
    uint32_t row;
    uint8_t bank;
    uint8_t channel;
    uint16_t rank;
  };

  Address() : data(0) {}
  Address(uint64_t a) : data(a) {}
  Address(uint8_t c, uint16_t r, uint8_t b, uint32_t ro)
      : row(ro), bank(b), channel(c), rank(r) {}
};

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

  // Statistics
  Stats readStat;
  Stats writeStat;

  double act_energy;
  double pre_energy;
  double read_energy;
  double write_energy;
  double ref_energy;
  double sref_energy;
  double window_energy;

  uint64_t lastResetAt;

 public:
  AbstractDRAM(ObjectData &);
  ~AbstractDRAM();

  virtual void read(Address address, uint16_t size, Event eid,
                    uint64_t data = 0) = 0;
  virtual void write(Address address, uint16_t size, Event eid,
                     uint64_t data = 0) = 0;

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::Memory::DRAM

#endif
