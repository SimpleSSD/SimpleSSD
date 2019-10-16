// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_MEM_SRAM_ABSTRACT_SRAM_HH__
#define __SIMPLESSD_MEM_SRAM_ABSTRACT_SRAM_HH__

#include <cinttypes>

#include "sim/object.hh"

namespace SimpleSSD::Memory::SRAM {

class AbstractSRAM : public AbstractRAM {
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

  Config::SRAMStructure *pStructure;

  // TODO: Add SRAM power model (from McPAT?)

  // double totalEnergy;  // Unit: pJ
  // double totalPower;   // Unit: mW
  Stats readStat;
  Stats writeStat;

  void rangeCheck(uint64_t, uint64_t) noexcept;

 public:
  AbstractSRAM(ObjectData &);
  virtual ~AbstractSRAM();

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::Memory::SRAM

#endif
