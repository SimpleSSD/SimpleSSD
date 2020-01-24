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
#include "util/stat_helper.hh"

namespace SimpleSSD::Memory::DRAM {

class AbstractDRAM : public Object {
 protected:
  Config::DRAMStructure *pStructure;
  Config::DRAMTiming *pTiming;
  Config::DRAMPower *pPower;

  void convertMemspec(Data::MemorySpecification &);

 public:
  AbstractDRAM(ObjectData &);
  ~AbstractDRAM();

  virtual void read(uint64_t, Event, uint64_t = 0) = 0;
  virtual void write(uint64_t, Event, uint64_t = 0) = 0;
};

}  // namespace SimpleSSD::Memory::DRAM

#endif
