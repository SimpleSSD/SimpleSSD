/*
 * Copyright (C) 2017 CAMELab
 *
 * This file is part of SimpleSSD.
 *
 * SimpleSSD is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * SimpleSSD is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with SimpleSSD.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __DRAM_SIMPLE__
#define __DRAM_SIMPLE__

#include "dram/abstract_dram.hh"

namespace SimpleSSD {

namespace DRAM {

class SimpleDRAM : public AbstractDRAM {
 private:
  Config::DRAMStructure *pStructure;
  Config::DRAMTiming *pTiming;
  Config::DRAMPower *pPower;

  uint64_t pageFetchLatency;
  double interfaceBandwidth;

  uint64_t lastDRAMAccess;

 public:
  SimpleDRAM(Config &p);
  ~SimpleDRAM();

  void updateDelay(uint64_t, uint64_t &);

  void read(uint64_t, uint64_t, uint64_t &) override;
  void write(uint64_t, uint64_t, uint64_t &) override;
};

}  // namespace DRAM

}  // namespace SimpleSSD

#endif
