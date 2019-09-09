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

#ifdef DEBUG_SIMPLESSD
#include <string>
#endif

namespace SimpleSSD {

#ifdef DEBUG_SIMPLESSD
struct _Event {
  uint64_t eid;
  std::string name;

  _Event() : eid(0) {}
};
#endif

using Tick = uint64_t;
using Event = uint64_t;
using EventFunction = std::function<void(uint64_t)>;

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
  virtual Tick getTick() = 0;

#ifdef DEBUG_SIMPLESSD
  //! Create event with SimpleSSD::EventFunction, with description.
  virtual Event createEvent(EventFunction, std::string) = 0;
#else
  //! Create event with SimpleSSD::EventFunction.
  virtual Event createEvent(EventFunction) = 0;
#endif
  //! Schedule event.
  virtual void schedule(Event, Tick) = 0;

  //! Deschedule event.
  virtual void deschedule(Event) = 0;

  //! Check event is scheduled or not.
  virtual bool isScheduled(Event) = 0;

  //! Remove event.
  virtual void removeEvent(Event) = 0;
};

}  // namespace SimpleSSD

#endif
