/*
 * Copyright (C) 2017 CAMELab
 *
 * This file is part of SimpleSSD.
 *
 * SimpleSSD is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * SimpleSSD is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with SimpleSSD.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#ifndef __SIM_SIMULATOR__
#define __SIM_SIMULATOR__

#include <cinttypes>
#include <functional>

namespace SimpleSSD {

typedef uint64_t Event;
typedef std::function<void(uint64_t)> EventFunction;

class Simulator {
 public:
  Simulator() {}
  virtual ~Simulator() {}

  virtual uint64_t getCurrentTick() = 0;

  virtual Event allocateEvent(EventFunction) = 0;
  virtual void scheduleEvent(Event, uint64_t) = 0;
  virtual void descheduleEvent(Event) = 0;
  virtual bool isScheduled(Event) = 0;
  virtual void deallocateEvent(Event) = 0;
};

// Defined in sim/simulator.cc
extern Simulator *sim;

inline void setSimulator(Simulator *p) {
  sim = p;
}

}  // namespace SimpleSSD

#endif
