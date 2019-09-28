// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIM_ENGINE_HH__
#define __SIM_ENGINE_HH__

#include <cinttypes>
#include <functional>
#include <string>

namespace SimpleSSD {

/**
 * \brief Event ID definition
 *
 * Unique ID of events in SimpleSSD. Event ID 0 is invalid.
 */
using Event = uint64_t;

const Event InvalidEventID = 0;

/**
 * \brief Event function definition
 *
 * Event function will be called when the event triggered.
 * First param is current tick, second param is user data passed in schedule
 * function.
 */
using EventFunction = std::function<void(uint64_t, void *)>;

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

  //! Schedule event.
  virtual void schedule(Event, uint64_t, void * = nullptr) = 0;

  //! Deschedule event.
  virtual void deschedule(Event) = 0;

  //! Check event is scheduled or not.
  virtual bool isScheduled(Event) = 0;

  //! Remove event.
  virtual void removeEvent(Event) = 0;
};

}  // namespace SimpleSSD

#endif
