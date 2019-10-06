// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_UTIL_INTERFACE__
#define __SIMPLESSD_UTIL_INTERFACE__

#include <cinttypes>
#include <functional>

namespace SimpleSSD {

using DelayFunction = std::function<uint64_t(uint64_t)>;

// SimpleSSD::PCIExpress
namespace PCIExpress {

enum class Generation : uint8_t {
  Gen1,  // PCI Express Gen. 1.x
  Gen2,  // PCI Express Gen. 2.x
  Gen3,  // PCI Express Gen. 3.x
  Size,
};

DelayFunction makeFunction(Generation, uint8_t);

}  // namespace PCIExpress

// SimpleSSD::SATA
namespace SATA {

enum class Generation : uint8_t {
  Gen1,  // SATA 1.0 (1.5Gbps)
  Gen2,  // SATA 2.0 (3Gbps)
  Gen3,  // SATA 3.0/3.1 (6Gbps)
  Size
};

DelayFunction makeFunction(Generation, uint8_t);

}  // namespace SATA

// SimpleSSD::MIPI
namespace MIPI {

// SimpleSSD:MIPI::M_PHY
namespace M_PHY {

enum class Mode : uint8_t {
  HighSpeed_Gear1,  // High Speed Gear 1
  HighSpeed_Gear2,  // High Speed Gear 2
  HighSpeed_Gear3,  // High Speed Gear 3
  HighSpeed_Gear4,  // High Speed Gear 4
  Size
};

DelayFunction makeFunction(M_PHY::Mode, uint8_t);

}  // namespace M_PHY

// SimpleSSD::MIPI::UniPro
namespace UniPro {

DelayFunction makeFunction(M_PHY::Mode, uint8_t);

}  // namespace UniPro

}  // namespace MIPI

// SimpleSSD::ARM
namespace ARM {

// SimpleSSD::ARM::AXI
namespace AXI {

enum class Width : uint16_t {
  Bit32 = 4,
  Bit64 = 8,
  Bit128 = 16,
  Bit256 = 32,
  Bit512 = 64,
  Bit1024 = 128,
};

DelayFunction makeFunction(uint64_t, Width);

// SimpleSSD::ARM::AXI::Stream
namespace Stream {

DelayFunction makeFunction(uint64_t, Width);

}  // namespace Stream

}  // namespace AXI

}  // namespace ARM

}  // namespace SimpleSSD

#endif
