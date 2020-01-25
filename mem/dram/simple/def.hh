// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_MEM_DRAM_SIMPLE_DEF_HH__
#define __SIMPLESSD_MEM_DRAM_SIMPLE_DEF_HH__

#include <cinttypes>

namespace SimpleSSD::Memory::DRAM::Simple {

enum class Command : uint8_t {
  Read,
  ReadAP,
  Write,
  WriteAP,
  Activate,
  Precharge,
  Refresh,
};

enum class BankState : uint8_t {
  Idle,
  Activate,
  Precharge,
  Refresh,
  PowerDown,
};

struct Packet {
  const uint64_t id;
  uint64_t finishedAt;

  Command opcode;
  uint8_t channel;
  uint8_t rank;
  uint8_t bank;
  uint32_t row;
  uint32_t column;

  Packet(uint64_t i)
      : id(i),
        finishedAt(0),
        opcode(Command::Read),
        channel(0),
        rank(0),
        bank(0),
        row(0),
        column(0) {}
  Packet(uint64_t i, Command c, uint8_t ch, uint8_t r, uint8_t b, uint32_t ro,
         uint32_t co)
      : id(i),
        finishedAt(0),
        opcode(c),
        channel(ch),
        rank(r),
        bank(b),
        row(ro),
        column(co) {}
};

}  // namespace SimpleSSD::Memory::DRAM::Simple

#endif
