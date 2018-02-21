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

}  // namespace SimpleSSD
