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

#include "util/interface.hh"

#include "util/algorithm.hh"

namespace SimpleSSD {

namespace PCIExpress {

static const uint32_t maxPayloadSize = 4096;                        // Bytes
static const uint32_t packetOverhead = 36;                          // Bytes
static const uint32_t internalDelay[PCIE_NUM] = {19, 70, 115};      // Symbol
static const float encoding[PCIE_NUM] = {1.25f, 1.25f, 1.015625f};  // Ratio
static const uint32_t delay[PCIE_NUM] = {3200, 1600, 1000};  // pico-second

uint64_t calculateDelay(PCIE_GEN gen, uint8_t lane, uint64_t bytesize) {
  uint64_t nTLP = MAX((bytesize - 1) / maxPayloadSize + 1, 1);
  uint64_t nSymbol;

  nSymbol = bytesize + nTLP * (packetOverhead);
  nSymbol = (nSymbol - 1) / lane + 2 + internalDelay[gen];

  return (uint64_t)(delay[gen] * encoding[gen] * nSymbol + 0.5f);
}

}  // namespace PCIExpress

namespace ARM {

namespace AXI {

// Two cycles per one data beat
// Two cycles for address, one cycle for write response
// One burst request should contain 1, 2, 4, 8 .. of beats

static const uint32_t maxPayloadSize = 4096;  // Bytes

uint64_t calculateDelay(uint64_t clock, BUS_WIDTH width, uint64_t bytesize) {
  uint32_t nBeats = MAX((bytesize - 1) / width + 1, 1);
  uint32_t nBursts = popcount(nBeats) + (bytesize - 1) / maxPayloadSize;
  uint32_t nClocks = nBeats * 2 + nBursts * 3;
  uint64_t period = (uint64_t)(1000000000000.0 / clock + 0.5);

  return nClocks * period;
}

namespace Stream {

// Unlimited bursts
// If master and slave can handle data sufficiently fast,
// one cycle per one data beat
// No address and responses

uint64_t calculateDelay(uint64_t clock, BUS_WIDTH width, uint64_t bytesize) {
  uint64_t period = (uint64_t)(1000000000000.0 / clock + 0.5);

  return bytesize / width * period;
}

}  // namespace Stream

}  // namespace AXI

}  // namespace ARM

}  // namespace SimpleSSD
