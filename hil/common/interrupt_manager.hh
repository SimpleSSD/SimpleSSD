// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __HIL_COMMON_INTERRUPT_MANAGER_HH__
#define __HIL_COMMON_INTERRUPT_MANAGER_HH__

#include <map>

#include "sim/interface.hh"
#include "sim/object.hh"

namespace SimpleSSD::HIL {

/**
 * \brief Interrupt manager object
 *
 * Manages interrupt posting. Also calculates interrupt coalescing.
 */
class InterruptManager : public Object {
 private:
  class CoalesceData {
   public:
    Event timerEvent;
    bool pending;
    uint16_t iv;
    uint16_t currentRequestCount;
    uint64_t nextDeadline;

    CoalesceData();
  };

  Interface *pInterface;

  bool interruptCoalescing;
  uint16_t aggregationThreshold;  // NVMe 8bit, AHCI 8bit
  uint64_t aggregationTime;  // NVMe 8bit * 100us unit, AHCI 16bit * 1ms unit

  std::map<uint16_t, CoalesceData> coalesceMap;

  void timerHandler(uint64_t, CoalesceData *);
  void enableTimer();
  void disableTimer();

 public:
  InterruptManager(ObjectData &, Interface *);
  InterruptManager(const InterruptManager &) = delete;
  InterruptManager(InterruptManager &&) noexcept = default;
  ~InterruptManager();

  InterruptManager &operator=(const InterruptManager &) = delete;
  InterruptManager &operator=(InterruptManager &&) noexcept = default;

  void postInterrupt(uint16_t, bool);

  void enableCoalescing(bool, uint16_t);
  void configureCoalescing(uint64_t, uint16_t);

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint() noexcept override;
  void restoreCheckpoint() noexcept override;
};

}  // namespace SimpleSSD::HIL

#endif
