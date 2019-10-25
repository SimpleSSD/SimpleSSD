// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_SIM_EVENT_HH__
#define __SIMPLESSD_SIM_EVENT_HH__

#include <cinttypes>
#include <functional>

#include "sim/checkpoint.hh"

namespace SimpleSSD {

class EventData;

/**
 * \brief Event ID definition
 *
 * Unique ID of events in SimpleSSD. Event ID 0 is invalid.
 */
using Event = EventData *;

const Event InvalidEventID = nullptr;

/**
 * \brief Event function definition
 *
 * Event function will be called when the event triggered.
 * First param is current tick, second param is user data passed in schedule
 * function.
 */
using EventFunction = std::function<void(uint64_t, uint64_t)>;

/**
 * \brief Interrupt function definition
 *
 * See SimpleSSD::Engine::setFunction for more details.
 */
using InterruptFunction = std::function<void(Event, uint64_t, uint64_t)>;

}  // namespace SimpleSSD

#endif
