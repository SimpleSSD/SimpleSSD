// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_FIL_NVM_ABSTRACT_NVM_HH__
#define __SIMPLESSD_FIL_NVM_ABSTRACT_NVM_HH__

#include <cinttypes>

#include "hil/command_manager.hh"
#include "sim/object.hh"

namespace SimpleSSD::FIL::NVM {

class AbstractNVM : public Object {
 protected:
  CommandManager *commandManager;

 public:
  AbstractNVM(ObjectData &o, CommandManager *m)
      : Object(o), commandManager(m) {}
  virtual ~AbstractNVM() {}

  /**
   * \brief Handle SubCommand
   *
   * This SubCommand must have valid ppn field.
   *
   * \param[in] tag Command tag
   * \param[in] id  SubCommand id
   */
  virtual void enqueue(uint64_t tag, uint32_t id) = 0;

  /**
   * \brief Write spare data without timing calculation
   *
   * This function should only be used in FTL initialization (warm-up)
   * procedure.
   */
  virtual void writeSpare(PPN, std::vector<uint8_t> &) = 0;
};

}  // namespace SimpleSSD::FIL::NVM

#endif
