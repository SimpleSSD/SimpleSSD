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

#ifndef __UTIL_INTERFACE__
#define __UTIL_INTERFACE__

#include <cinttypes>

namespace SimpleSSD {

namespace PCIExpress {

typedef enum {
  PCIE_1_X,  // PCI Express Gen. 1.x
  PCIE_2_X,  // PCI Express Gen. 2.x
  PCIE_3_X,  // PCI Express Gen. 3.x
  PCIE_NUM
} PCIE_GEN;

uint64_t calculateDelay(PCIE_GEN, uint8_t, uint64_t);

}  // namespace PCIExpress

}  // namespace SimpleSSD

#endif
