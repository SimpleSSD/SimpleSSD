// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_MEM_ABSTRACT_RAM_HH__
#define __SIMPLESSD_MEM_ABSTRACT_RAM_HH__

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
   * \param[in] data    Event data
   */
  virtual void read(uint64_t address, uint64_t length, Event eid,
                    uint64_t data = 0) = 0;

  /**
   * \brief Write SRAM
   *
   * Write SRAM with callback event.
   *
   * \param[in] address Begin address of SRAM
   * \param[in] length  Amount of data to write
   * \param[in] eid     Event ID of callback event
   * \param[in] data    Event data
   */
  virtual void write(uint64_t address, uint64_t length, Event eid,
                     uint64_t data = 0) = 0;

  /**
   * \brief Allocate range of RAM
   *
   * Allocate a portion of memory address range. If no space available, it
   * panic. (You need to configure larger RAM for firmware.)
   *
   * \param[in] size  Requested memory size
   * \return address  Beginning address of allocated range
   */
  virtual uint64_t allocate(uint64_t size) = 0;
};

class Request {
 public:
  uint64_t offset;
  uint64_t length;
  Event eid;
  uint64_t data;
  uint64_t beginAt;

  Request() : offset(0), length(0), eid(InvalidEventID) {}
  Request(uint64_t a, uint64_t l, Event e, uint64_t d)
      : offset(a), length(l), eid(e), data(d) {}
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
