// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_MEM_DRAM_LPDDR4_HH__
#define __SIMPLESSD_MEM_DRAM_LPDDR4_HH__

#include <map>

#include "mem/dram/abstract_dram.hh"

namespace SimpleSSD::Memory::DRAM {

class LPDDR4 : public AbstractDRAM {
 private:
  enum class BankState : uint8_t {
    Precharge,
    Activate,
  };

  struct Bank {
    BankState state;

    uint32_t row;

    uint64_t lastACT;
    uint64_t lastPRE;
    uint64_t lastREAD;
    uint64_t lastWRITE;

    Bank();
  };

  struct Rank {
    uint32_t activeBank;

    libDRAMPower *power;

    std::vector<Bank> banks;

    Rank();

    // Stat
    uint64_t readRowHit;
    uint64_t readCount;
    uint64_t readBytes;
    uint64_t writeRowHit;
    uint64_t writeCount;
    uint64_t writeBytes;
  };

  std::vector<Rank> ranks;

  struct Entry {
    Event eid;
    uint64_t data;

    Entry(Event e, uint64_t d) : eid(e), data(d) {}
  };

  std::multimap<uint64_t, Entry> requestQueue;

  Event eventCompletion;

  void completeRequest(uint64_t);
  void reschedule();

  uint64_t wtr;
  uint64_t rtw;
  uint64_t rtr;
  uint64_t wtw;

 public:
  LPDDR4(ObjectData &);
  ~LPDDR4();

  bool isIdle(uint32_t, uint8_t) override;
  uint32_t getRowInfo(uint32_t, uint8_t) override;
  void submit(Address, uint32_t, bool, Event, uint64_t) override;

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::Memory::DRAM

#endif
