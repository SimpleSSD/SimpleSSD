// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_MEM_DRAM_SIMPLE_SIMPLE_HH__
#define __SIMPLESSD_MEM_DRAM_SIMPLE_SIMPLE_HH__

#include "mem/def.hh"
#include "mem/dram/abstract_dram.hh"
#include "util/scheduler.hh"

namespace SimpleSSD::Memory::DRAM::Simple {

/**
 * \brief Simple DRAM model
 *
 * Simple DRAM model with Rank/Bank/Row/Col calculation.
 * Inspired by DRAMSim2.
 */
class SimpleDRAM : public AbstractDRAM {
 private:
  union Address {
    uint64_t data;
    struct {
      uint32_t row;
      uint8_t bank;
      uint8_t channel;
      uint16_t rank;
    };

    Address() : data(0) {}
    Address(uint64_t a) : data(a) {}
    Address(uint8_t c, uint16_t r, uint8_t b, uint32_t ro)
        : row(ro), bank(b), channel(c), rank(r) {}
  };

  Address addressLimit;
  std::function<Address(uint64_t)> decodeAddress;

  SingleScheduler<Request *> scheduler;

  uint64_t activateLatency;
  uint64_t ioReadLatency;
  uint64_t ioWriteLatency;
  uint64_t burstLatency;
  uint64_t burstSize;
  uint64_t burstInRow;

  uint64_t preSubmit(Request *);
  void postDone(Request *);

  std::vector<std::vector<std::vector<uint32_t>>> rowOpened;

  bool checkRow(uint64_t);

  CountStat readHit;
  CountStat writeHit;
  BusyStat readBusy;
  BusyStat writeBusy;
  BusyStat totalBusy;

 public:
  SimpleDRAM(ObjectData &);
  ~SimpleDRAM();

  void read(uint64_t, Event, uint64_t = 0);
  void write(uint64_t, Event, uint64_t = 0);

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::Memory::DRAM::Simple

#endif
