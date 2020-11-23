// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_FIL_NVM_COMMON_LUN_HH__
#define __SIMPLESSD_FIL_NVM_COMMON_LUN_HH__

#include "sim/object.hh"

namespace SimpleSSD::FIL::NVM {

enum class State : uint8_t {
  Idle,       // Non-operational
  PreDMA,     // Command + (data)
  Operation,  // Memory operation
  PostDMA,    // Respone + (data)
  Invalid,    // Invalid state
};

class LUN : public Object {
 protected:
  State state;
  State nextState;
  uint64_t transitAt;

 public:
  LUN(ObjectData &);
  virtual ~LUN();

  State getCurrentState() noexcept;
  State getNextState() noexcept;

  virtual void transit(State, uint64_t) = 0;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::FIL::NVM

#endif
