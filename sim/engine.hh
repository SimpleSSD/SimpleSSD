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
 * Simulation engine object. SimpleSSD will use this object to access
 * simulation related APIs, such as event scheduling and checkpointing.
 */
class Engine {
 public:
  Engine() {}
  virtual ~Engine() {}

  //! Return current simulation tick in pico-second unit
  virtual uint64_t getTick() = 0;

  //! Create event with SimpleSSD::EventFunction, with description.
  virtual Event createEvent(EventFunction, std::string) = 0;

  /**
   * \brief Schedule event object
   *
   * Schedule event at provided tick. If tick < current tick, just schedule at
   * current tick. Reschedule if event is already scheduled.
   */
  virtual void schedule(Event, uint64_t) = 0;

  /**
   * \brief Deschedule event object
   *
   * Deschedule event. Ignore if event is not scheduled.
   */
  virtual void deschedule(Event) = 0;

  //! Check event is scheduled or not.
  virtual bool isScheduled(Event) = 0;

  /**
   * \brief Destroy event object
   *
   * Destroy event. Deschedule event if scheduled before destroy.
   */
  virtual void destroyEvent(Event) = 0;

  /**
   * \brief Create checkpoint
   *
   * Store all event ID, scheduled time and name.
   */
  virtual void createCheckpoint(std::ostream &) = 0;

  /**
   * \brief Restore from checkpoint
   *
   * Restore all event ID, scheduled time and name.
   * Must validate created event ID and restored event ID.
   */
  virtual void restoreCheckpoint(std::istream &) = 0;
};

}  // namespace SimpleSSD

#endif
