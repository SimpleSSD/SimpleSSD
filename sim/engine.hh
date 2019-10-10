// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_SIM_ENGINE_HH__
#define __SIMPLESSD_SIM_ENGINE_HH__

#include <string>

#include "sim/event.hh"

namespace SimpleSSD {

/**
 * \brief Engine object declaration
 *
 * Simulation engine object. SimpleSSD::CPU will use this object to manage
 * events.
 */
class Engine {
 public:
  Engine() {}
  virtual ~Engine() {}

  /**
   * \brief Set event function
   */
  virtual void setFunction(EventFunction) = 0;

  /**
   * \brief Schedule event function
   *
   * Schedule event at provided tick. Reschedule if event is already scheduled.
   */
  virtual void schedule(uint64_t) = 0;
};

}  // namespace SimpleSSD

#endif
