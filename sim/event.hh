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

#include "sim/checkpoint.hh"

namespace SimpleSSD {

/**
 * \brief Event ID definition
 *
 * Unique ID of events in SimpleSSD. Event ID 0 is invalid.
 */
using Event = uint64_t;

const Event InvalidEventID = 0;

class EventContext {
 private:
  uint64_t size;
  void *data;

 public:
  EventContext() : size(0), data(nullptr) {}

  template <class Type>
  EventContext(Type t) noexcept = delete;

  template <class Type>
  EventContext(Type *t) noexcept : size(sizeof(Type)), data(t) {}

  template <class Type, std::enable_if_t<!std::is_pointer_v<Type>>* = nullptr>
  Type get() noexcept = delete;

  template <class Type, std::enable_if_t<std::is_pointer_v<Type>>* = nullptr>
  Type get() noexcept {
    return (Type)data;
  }

  void backup(std::ostream &out) noexcept {
    BACKUP_SCALAR(out, size);

    if (size > 0) {
      BACKUP_BLOB(out, data, size);
    }
  }

  void restore(std::istream &in) noexcept {
    RESTORE_SCALAR(in, size);

    if (size > 0) {
      data = malloc(size);

      RESTORE_BLOB(in, data, size);
    }
  }
};

/**
 * \brief Event function definition
 *
 * Event function will be called when the event triggered.
 * First param is current tick, second param is user data passed in schedule
 * function.
 */
using EventFunction = std::function<void(uint64_t, EventContext)>;

class Request {
 public:
  uint64_t offset;
  uint64_t length;
  Event eid;
  EventContext context;
  uint64_t beginAt;

  Request() : offset(0), length(0), eid(InvalidEventID) {}
  Request(uint64_t a, uint64_t l, Event e, EventContext c)
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
  RequestWithData(uint64_t a, uint64_t l, Event e, EventContext c, uint8_t *b)
      : Request(a, l, e, c), buffer(b) {}
  RequestWithData(const RequestWithData &) = delete;
  RequestWithData(RequestWithData &&) noexcept = default;

  RequestWithData &operator=(const RequestWithData &) = delete;
  RequestWithData &operator=(RequestWithData &&) = default;
};

}  // namespace SimpleSSD

#endif
