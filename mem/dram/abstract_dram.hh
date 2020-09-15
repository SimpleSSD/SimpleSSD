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

#include "sim/object.hh"
#include "util/stat_helper.hh"

namespace SimpleSSD::Memory::DRAM {

class AbstractDRAM : public Object {
 protected:
  Config::DRAMStructure *pStructure;
  Config::DRAMTiming *pTiming;
  Config::DRAMPower *pPower;

  SizeStat readStat;
  SizeStat writeStat;

 public:
  AbstractDRAM(ObjectData &);
  ~AbstractDRAM();

  virtual void read(uint64_t, Event, uint64_t = 0) = 0;
  virtual void write(uint64_t, Event, uint64_t = 0) = 0;

  virtual uint64_t size() noexcept;

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::Memory::DRAM

#endif
