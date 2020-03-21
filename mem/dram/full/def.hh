// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_MEM_DRAM_SIMPLE_DEF_HH__
#define __SIMPLESSD_MEM_DRAM_SIMPLE_DEF_HH__

#include "mem/config.hh"

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
};

struct Timing {
  uint32_t readToPre;
  uint32_t readAP;
  uint32_t readToRead;
  uint32_t readToWrite;
  uint32_t readToComplete;
  uint32_t writeToPre;
  uint32_t writeAP;
  uint32_t writeToRead;
  uint32_t writeToWrite;
  uint32_t tRCD;
  uint32_t tRFC;
  uint32_t tRP;
  uint32_t tBL;
  uint32_t tCK;

  Timing(Config::DRAMStructure *dram, Config::DRAMTiming *timing) {
    tCK = timing->tCK;
    tBL = dram->burstLength / 2 * tCK;

    readToPre = tBL / 2 + timing->tRTP;
    readAP = tBL / 2 + timing->tRTP + timing->tRP;
    readToRead = 2 * timing->tCCD;
    readToWrite = timing->tRL + timing->tDQSCK + tBL / 2 - timing->tWL;
    readToComplete = timing->tRL + timing->tDQSCK + tBL / 2;
    writeToPre = timing->tWL + tBL / 2 + tCK + timing->tWR;
    writeAP = timing->tWL + tBL / 2 + tCK + timing->tWR + timing->tRP;
    writeToRead = timing->tWL + tBL / 2 + tCK + timing->tWTR;
    writeToWrite = 2 * timing->tCCD;
    tRCD = timing->tRCD;
    tRFC = timing->tRFC;
    tRP = timing->tRP;
  }
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
