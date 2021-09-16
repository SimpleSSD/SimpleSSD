// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_HIL_COMMON_INTERRUPT_MANAGER_HH__
#define __SIMPLESSD_HIL_COMMON_INTERRUPT_MANAGER_HH__

#include <map>

#include "sim/abstract_subsystem.hh"
#include "sim/interface.hh"
#include "sim/object.hh"
#include "util/sorted_map.hh"

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
    bool pending;
    uint16_t currentRequestCount;
    uint64_t nextDeadline;

    CoalesceData();
  };

  Interface *pInterface;
  ControllerID controllerID;

  uint16_t aggregationThreshold;  // NVMe 8bit, AHCI 8bit
  uint64_t aggregationTime;  // NVMe 8bit * 100us unit, AHCI 16bit * 1ms unit

  map_map<uint16_t, CoalesceData> coalesceMap;

  Event eventTimer;

  void timerHandler(uint64_t);
  void reschedule(map_map<uint16_t, CoalesceData>::iterator);

 public:
  InterruptManager(ObjectData &, Interface *, ControllerID);
  ~InterruptManager();

  void postInterrupt(uint16_t, bool);

  void enableCoalescing(bool, uint16_t);
  bool isEnabled(uint16_t);
  void configureCoalescing(uint64_t, uint16_t);
  void getCoalescing(uint64_t &, uint16_t &);

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::HIL

#endif
