// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __MEM_ABSTRACT_RAM__
#define __MEM_ABSTRACT_RAM__

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
   * \param[in] context User data of callback
   */
  virtual void read(uint64_t address, uint64_t length, Event eid,
                    EventContext context = EventContext()) = 0;

  /**
   * \brief Write SRAM
   *
   * Write SRAM with callback event.
   *
   * \param[in] address Begin address of SRAM
   * \param[in] length  Amount of data to write
   * \param[in] eid     Event ID of callback event
   * \param[in] context User data of callback
   */
  virtual void write(uint64_t address, uint64_t length, Event eid,
                     EventContext context = EventContext()) = 0;
};

}  // namespace SimpleSSD::Memory

#endif
