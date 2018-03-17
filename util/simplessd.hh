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

#ifndef __UTIL_SIMPLESSD__
#define __UTIL_SIMPLESSD__

#include "sim/config_reader.hh"
#include "sim/simulator.hh"
#include "sim/statistics.hh"
#include "sim/trace.hh"

namespace SimpleSSD {

inline uint64_t getTick() {
  return sim->getCurrentTick();
}

inline Event allocate(EventFunction f) {
  return sim->allocateEvent(f);
}

inline void schedule(Event e, uint64_t t) {
  sim->scheduleEvent(e, t);
}

inline void deschedule(Event e) {
  sim->descheduleEvent(e);
}

inline bool scheduled(Event e) {
  return sim->isScheduled(e);
}

inline void deallocate(Event e) {
  sim->deallocateEvent(e);
}

}  // namespace SimpleSSD

#endif
