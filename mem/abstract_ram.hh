// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_MEM_ABSTRACT_RAM__
#define __SIMPLESSD_MEM_ABSTRACT_RAM__

#include "sim/object.hh"

namespace SimpleSSD::Memory {

class AbstractRAM : public Object {
 public:
  AbstractRAM(ObjectData &o) : Object(o) {}
  virtual ~AbstractRAM() {}

  /**
   * \brief Read SRAM
   *
   * Read SRAM with callback event.
   *
   * \param[in] address Begin address of SRAM
   * \param[in] length  Amount of data to read
   * \param[in] eid     Event ID of callback event
   */
  virtual void read(uint64_t address, uint64_t length, Event eid) = 0;

  /**
   * \brief Write SRAM
   *
   * Write SRAM with callback event.
   *
   * \param[in] address Begin address of SRAM
   * \param[in] length  Amount of data to write
   * \param[in] eid     Event ID of callback event
   */
  virtual void write(uint64_t address, uint64_t length, Event eid) = 0;
};

class Request {
 public:
  uint64_t offset;
  uint64_t length;
  Event eid;
  uint64_t beginAt;

  Request() : offset(0), length(0), eid(InvalidEventID) {}
  Request(uint64_t a, uint64_t l, Event e) : offset(a), length(l), eid(e) {}
  Request(const Request &) = delete;
  Request(Request &&) noexcept = default;

  Request &operator=(const Request &) = delete;
  Request &operator=(Request &&) = default;

  static void backup(std::ostream &out, Request *item) {
    BACKUP_EVENT(out, item->eid);
    BACKUP_SCALAR(out, item->offset);
    BACKUP_SCALAR(out, item->length);
    BACKUP_SCALAR(out, item->beginAt);
  }

  static Request *restore(std::istream &in, ObjectData &object) {
    auto item = new Request();

    RESTORE_EVENT(in, item->eid);
    RESTORE_SCALAR(in, item->offset);
    RESTORE_SCALAR(in, item->length);
    RESTORE_SCALAR(in, item->beginAt);

    return item;
  }
};

}  // namespace SimpleSSD::Memory

#endif
