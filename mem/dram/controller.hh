// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_MEM_DRAM_DRAM_CONTROLLER_HH__
#define __SIMPLESSD_MEM_DRAM_DRAM_CONTROLLER_HH__

#include "mem/abstract_ram.hh"
#include "mem/dram/abstract_dram.hh"

namespace SimpleSSD::Memory::DRAM {

class Channel : public Object {
 private:
  const uint8_t id;

  AbstractDRAM *pDRAM;

  // Statistics
  uint64_t readCount;
  uint64_t readFromWriteQueue;
  uint64_t readBytes;
  uint64_t writeCount;
  uint64_t writeMerged;
  uint64_t writeBytes;

 public:
  Channel(ObjectData &, uint8_t);
  ~Channel();

  void read(Address address, uint16_t size, Event eid, uint64_t data = 0);
  void write(Address address, uint16_t size, Event eid, uint64_t data = 0);

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

class DRAMController : public AbstractRAM {
 private:
  Config::DRAMController *ctrl;

  // DRAM Channels
  std::vector<Channel *> channels;

  // Address decoder
  uint64_t capacity;
  Address addressLimit;

  std::function<Address(uint64_t)> decodeAddress;

  // Completion handler

  // Stat bin
  struct StatisticBin {
    // Address range
    const uint64_t begin;
    const uint64_t end;

    // Statistics
    uint64_t lastResetAt;

    uint64_t readCount;
    uint64_t writeCount;
    uint64_t readBytes;
    uint64_t writeBytes;

    double readLatencySum;
    double writeLatencySum;

    StatisticBin(uint64_t, uint64_t);

    bool inRange(uint64_t, uint64_t);

    void getStatList(std::vector<Stat> &, std::string) noexcept;
    void getStatValues(std::vector<double> &, uint64_t) noexcept;
    void resetStatValues(uint64_t) noexcept;
  };

  std::vector<StatisticBin> statbin;

 public:
  DRAMController(ObjectData &);
  ~DRAMController();

  void read(uint64_t address, uint32_t length, Event eid,
            uint64_t data = 0) override;
  void write(uint64_t address, uint32_t length, Event eid,
             uint64_t data = 0) override;
  uint64_t allocate(uint64_t, std::string &&, bool = false) override;

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::Memory::DRAM

#endif
