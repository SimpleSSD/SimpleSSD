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
#include "util/stat_helper.hh"

namespace SimpleSSD::Memory::SRAM {

class AbstractSRAM : public Object {
 protected:
  Config::SRAMStructure *pStructure;

  double totalEnergy;   // Unit: pJ
  double averagePower;  // Unit: mW
  IOStat readStat;
  IOStat writeStat;

 public:
  AbstractSRAM(ObjectData &);
  virtual ~AbstractSRAM();

  virtual void read(uint64_t, Event, uint64_t = 0) = 0;
  virtual void write(uint64_t, Event, uint64_t = 0) = 0;

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::Memory::SRAM

#endif
