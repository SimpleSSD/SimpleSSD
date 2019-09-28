// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIM_EVENT_HH__
#define __SIM_EVENT_HH__

#include <cinttypes>
#include <functional>

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

class Request {
 public:
  uint64_t offset;
  uint64_t length;
  Event eid;
  void *context;
  uint64_t beginAt;

  Request() : offset(0), length(0), eid(InvalidEventID), context(nullptr) {}
  Request(uint64_t a, uint64_t l, Event e, void *c)
      : offset(a), length(l), eid(e), context(c) {}
  Request(const Request &) = delete;
  Request(Request &&) noexcept = default;

  Request &operator=(const Request &) = delete;
  Request &operator=(Request &&) = default;
};

class RequestWithData : public Request {
 public:
  uint8_t *buffer;

  RequestWithData() : Request(), buffer(nullptr) {}
  RequestWithData(uint64_t a, uint64_t l, Event e, void *c, uint8_t *b)
      : Request(a, l, e, c), buffer(b) {}
  RequestWithData(const RequestWithData &) = delete;
  RequestWithData(RequestWithData &&) noexcept = default;

  RequestWithData &operator=(const RequestWithData &) = delete;
  RequestWithData &operator=(RequestWithData &&) = default;
};

}  // namespace SimpleSSD

#endif
