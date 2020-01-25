// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_MEM_DRAM_SIMPLE_RANK_HH__
#define __SIMPLESSD_MEM_DRAM_SIMPLE_RANK_HH__

#include <deque>

#include "mem/dram/simple/def.hh"
#include "sim/object.hh"

namespace SimpleSSD::Memory::DRAM::Simple {

class Rank;

/**
 * \brief Bank parameters
 */
class BankStatus {
 private:
  friend Rank;

  BankState state;

  uint32_t activatedRowIndex;
  uint64_t nextRead;
  uint64_t nextWrite;
  uint64_t nextActivate;
  uint64_t nextPrecharge;
  uint64_t nextPowerUp;

 public:
  BankStatus();
  ~BankStatus();

  void createCheckpoint(std::ostream &) const noexcept;
  void restoreCheckpoint(std::istream &) noexcept;
};

/**
 * \brief Simple Rank model
 *
 * Simple DRAM Rank with bank state machine.
 */
class Rank : public Object {
 private:
  // TODO: Add parent

  bool pendingRefresh;

  std::vector<BankStatus> banks;

  // Read completion queue
  std::deque<Packet *> completionQueue;
  Event eventCompletion;

  Config::DRAMTiming *timing;

  // Timing shortcuts
  uint64_t readToPre;
  uint64_t readAP;
  uint64_t readToRead;
  uint64_t readToWrite;
  uint64_t readToComplete;
  uint64_t writeToPre;
  uint64_t writeAP;
  uint64_t writeToRead;
  uint64_t writeToWrite;
  uint64_t actToActSame;

  void completion(uint64_t);
  void updateCompletion();

 public:
  Rank(ObjectData &);
  ~Rank();

  void submit(Packet *);

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::Memory::DRAM::Simple

#endif
