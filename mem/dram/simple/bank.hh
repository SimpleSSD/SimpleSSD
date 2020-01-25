// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_MEM_DRAM_SIMPLE_BANK_HH__
#define __SIMPLESSD_MEM_DRAM_SIMPLE_BANK_HH__

#include <list>
#include <utility>

#include "mem/dram/simple/def.hh"
#include "sim/object.hh"
#include "util/stat_helper.hh"

namespace SimpleSSD::Memory::DRAM::Simple {

class Rank;

class Bank : public Object {
 private:
  Rank *parent;
  Timing *timing;

  const uint8_t bankID;

  Packet *currentPacket;
  uint64_t prevPacketAt;
  bool prevPacketWasRead;

  BankState state;

  uint32_t activatedRowIndex;

  std::list<std::pair<uint64_t, uint64_t>> completionQueue;

  CountStat readStat;
  CountStat writeStat;

  void updateWork();
  void work(uint64_t);

  Event eventWork;

  void updateCompletion();
  void completion(uint64_t);

  Event eventReadDone;

 public:
  Bank(ObjectData &, uint8_t, Rank *, Timing *);
  ~Bank();

  bool submit(Packet *);

  uint32_t getActiveRow();

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::Memory::DRAM::Simple

#endif
