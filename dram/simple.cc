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

#include "dram/simple.hh"

namespace SimpleSSD {

namespace DRAM {

SimpleDRAM::SimpleDRAM(Config &p) : AbstractDRAM(p), lastDRAMAccess(0) {
  pStructure = conf.getDRAMStructure();
  pTiming = conf.getDRAMTiming();
  pPower = conf.getDRAMPower();

  pageFetchLatency = pTiming->tRP + pTiming->tRCD + pTiming->tCL;
  interfaceBandwidth =
      2.0 * pStructure->busWidth * pStructure->channel / 8.0 / pTiming->tCK;
}

SimpleDRAM::~SimpleDRAM() {
  // DO NOTHING
}

void SimpleDRAM::updateDelay(uint64_t latency, uint64_t &tick) {
  if (tick > 0) {
    if (lastDRAMAccess <= tick) {
      lastDRAMAccess = tick + latency;
    }
    else {
      lastDRAMAccess += latency;
    }

    tick = lastDRAMAccess;
  }
}

void SimpleDRAM::read(void *addr, uint64_t size, uint64_t &tick) {
  uint64_t pageCount = (size > 0) ? (size - 1) / pStructure->pageSize + 1 : 0;
  uint64_t latency = (uint64_t)(
      pageFetchLatency + pageCount * pStructure->pageSize / interfaceBandwidth);

  updateDelay(latency, tick);
}

void SimpleDRAM::write(void *addr, uint64_t size, uint64_t &tick) {
  uint64_t pageCount = (size > 0) ? (size - 1) / pStructure->pageSize + 1 : 0;
  uint64_t latency = (uint64_t)(
      pageFetchLatency + pageCount * pStructure->pageSize / interfaceBandwidth);

  updateDelay(latency, tick);
}

}  // namespace DRAM

}  // namespace SimpleSSD
