// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "util/interface.hh"

#include "util/algorithm.hh"

namespace SimpleSSD {

namespace PCIExpress {

//! Maximum payload size in bytes
const uint32_t maxPayloadSize = 4096;
//! TLP overhead (header and etc) in bytes
const uint32_t packetOverhead = 36;
//! Internal delay in symbol unit defined in spec
const uint32_t internalDelay[(uint8_t)Generation::Size] = {19, 70, 115};
//! Encoding ratio
const float encoding[(uint8_t)Generation::Size] = {1.25f, 1.25f, 1.015625f};
//! Required time to transfer one symbol in ps
const uint32_t delay[(uint8_t)Generation::Size] = {3200, 1600, 1000};

//! Make PCI Express delay function
DelayFunction makeFunction(Generation gen, uint8_t lane) {
  return [lane, mp = maxPayloadSize, po = packetOverhead,
          id = internalDelay[(uint8_t)gen], en = encoding[(uint8_t)gen],
          d = delay[(uint8_t)gen]](uint64_t length) -> uint64_t {
    uint64_t nTLP = MAX(DIVCEIL(length, mp), 1);
    uint64_t nSymbol = length + nTLP * po;

    nSymbol = DIVCEIL(nSymbol, lane) + 1 + nTLP * id;

    return (uint64_t)(d * en * nSymbol + 0.5f);
  };
}

}  // namespace PCIExpress

namespace SATA {

// Each primitive is 1 DWORD (4bytes)
// One Fram contains SOF/EOF/CRC and HOLD/A primitives
// Just assume no HOLD/HOLDA

const uint32_t packetOverhead = 3;  // Primitive
const uint32_t internalDelay = 12;  // DWORD
const float encoding = 1.25f;       // Ratio
const uint32_t delay[(uint8_t)Generation::Size] = {5336, 2667, 1333};  // ps

//! Make SATA delay function
DelayFunction makeFunction(Generation gen) {
  return [id = internalDelay, po = packetOverhead, en = encoding,
          d = delay[(uint8_t)gen]](uint64_t length) -> uint64_t {
    uint64_t dwords = DIVCEIL(length, 4) + id + po;

    return (uint64_t)(d * en * dwords * 4 + 0.5f);
  };
}

}  // namespace SATA

namespace MIPI {

namespace M_PHY {

// See M-PHY spec v4.0 section 4.7.2 BURST, Figure 14 and 15
const uint32_t tLine = 7000;                                          // ps
const uint32_t nPrepare = 15;                                         // Bytes
const uint32_t nSync = 15;                                            // Bytes
const uint32_t nStall = 7;                                            // Bytes
const float encoding = 1.25f;                                         // Ratio
const uint32_t delay[(uint8_t)Mode::Size] = {6410, 3205, 1603, 801};  // ps

//! Make MIPI::M_PHY delay function
DelayFunction makeFunction(Mode mode, uint8_t lane) {
  return
      [lane, tl = tLine, p = nPrepare, sy = nSync, st = nStall, en = encoding,
       d = delay[(uint8_t)mode]](uint64_t symbol) -> uint64_t {
        uint64_t nSymbol = p + sy + 1 + symbol * 2 + st;

        nSymbol = DIVCEIL(nSymbol, lane) + 1;

        return (uint64_t)(d * en * nSymbol + tl + 0.5f);
      };
}

}  // namespace M_PHY

namespace UniPro {

const uint32_t maxPayloadSize = 254;  // Bytes
const uint32_t packetOverhead = 7;    // Symbol

//! Make MIPI::UniPro delay function
DelayFunction makeFunction(M_PHY::Mode mode, uint8_t lane) {
  return
      [func = M_PHY::makeFunction(mode, lane),  // Copy
       mp = maxPayloadSize, po = packetOverhead](uint64_t length) -> uint64_t {
        uint64_t nPacket = MAX(DIVCEIL(length, mp), 1);
        uint64_t nSymbol = (length + 1) / 2;

        nSymbol += nPacket * po;

        return func(nSymbol);
      };
}

}  // namespace UniPro

}  // namespace MIPI

namespace ARM {

namespace AXI {

// Two cycles per one data beat
// Two cycles for address, one cycle for write response
// One burst request should contain 1, 2, 4, 8 .. of beats

const uint32_t maxPayloadSize = 4096;  // Bytes

//! Make ARM::AXI delay function
DelayFunction makeFunction(uint64_t clock, Width width) {
  return [mp = maxPayloadSize, w = (uint8_t)width,
          period = (uint64_t)(1000000000000.0 / clock + 0.5)](
             uint64_t length) -> uint64_t {
    uint32_t nBeats = (uint32_t)MAX(DIVCEIL(length, w), 1);
    uint32_t nBursts = popcount32(nBeats) + (uint32_t)(length - 1) / mp;
    uint32_t nClocks = nBeats * 2 + nBursts * 3;

    return nClocks * period;
  };
}

namespace Stream {

// Unlimited bursts
// If master and slave can handle data sufficiently fast,
// one cycle per one data beat
// No address and responses

DelayFunction makeFunction(uint64_t clock, Width width) {
  return
      [w = (uint8_t)width, period = (uint64_t)(1000000000000.0 / clock + 0.5)](
          uint64_t length) -> uint64_t { return length / w * period; };
}

}  // namespace Stream

}  // namespace AXI

}  // namespace ARM

}  // namespace SimpleSSD
