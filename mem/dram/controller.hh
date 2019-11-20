// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_MEM_DRAM_DRAM_CONTROLLER_HH__
#define __SIMPLESSD_MEM_DRAM_DRAM_CONTROLLER_HH__

#include "mem/abstract_ram.hh"
#include "mem/dram/abstract_dram.hh"

namespace SimpleSSD::Memory::DRAM {

class DRAMController : public AbstractRAM {
 private:
  AbstractDRAM *pDRAM;

 public:
  DRAMController(ObjectData &);
  ~DRAMController();

  void read(uint64_t address, uint32_t length, Event eid,
            uint64_t data = 0) override;
  void write(uint64_t address, uint32_t length, Event eid,
             uint64_t data = 0) override;

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::Memory::DRAM

#endif
