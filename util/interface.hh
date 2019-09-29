// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __UTIL_INTERFACE__
#define __UTIL_INTERFACE__

#include <cinttypes>

namespace SimpleSSD {

// SimpleSSD::PCIExpress
namespace PCIExpress {

typedef enum {
  PCIE_1_X,  // PCI Express Gen. 1.x
  PCIE_2_X,  // PCI Express Gen. 2.x
  PCIE_3_X,  // PCI Express Gen. 3.x
  PCIE_NUM
} PCIE_GEN;

uint64_t calculateDelay(PCIE_GEN, uint8_t, uint64_t);

}  // namespace PCIExpress

// SimpleSSD::SATA
namespace SATA {

typedef enum {
  SATA_1_0,  // SATA 1.0 (1.5Gbps)
  SATA_2_0,  // SATA 2.0 (3Gbps)
  SATA_3_0,  // SATA 3.0/3.1 (6Gbps)
  SATA_NUM
} SATA_GEN;

uint64_t calculateDelay(SATA_GEN, uint64_t);

}  // namespace SATA

// SimpleSSD::MIPI
namespace MIPI {

// SimpleSSD:MIPI::M_PHY
namespace M_PHY {

typedef enum {
  HS_G1,  // High Speed Gear 1
  HS_G2,  // High Speed Gear 2
  HS_G3,  // High Speed Gear 3
  HS_G4,  // High Speed Gear 4
  HS_NUM
} M_PHY_MODE;

uint64_t calculateDelay(M_PHY_MODE, uint8_t, uint64_t);

}  // namespace M_PHY

// SimpleSSD::MIPI::UniPro
namespace UniPro {

uint64_t calculateDelay(M_PHY::M_PHY_MODE, uint8_t, uint64_t);

}  // namespace UniPro

}  // namespace MIPI

// SimpleSSD::ARM
namespace ARM {

// SimpleSSD::ARM::AXI
namespace AXI {

typedef enum {
  BUS_32BIT = 4,
  BUS_64BIT = 8,
  BUS_128BIT = 16,
  BUS_256BIT = 32,
  BUS_512BIT = 64,
  BUS_1024BIT = 128,
} BUS_WIDTH;

uint64_t calculateDelay(uint64_t, BUS_WIDTH, uint64_t);

// SimpleSSD::ARM::AXI::Stream
namespace Stream {

uint64_t calculateDelay(uint64_t, BUS_WIDTH, uint64_t);

}  // namespace Stream

}  // namespace AXI

}  // namespace ARM

}  // namespace SimpleSSD

#endif
