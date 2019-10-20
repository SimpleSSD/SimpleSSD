// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_FIL_ABSTRACT_NVM_HH__
#define __SIMPLESSD_FIL_ABSTRACT_NVM_HH__

#include <cinttypes>

#include "hil/command_manager.hh"
#include "sim/object.hh"

namespace SimpleSSD::FIL::NVM {

class AbstractNVM : public Object {
 protected:
  HIL::CommandManager *commandManager;

 public:
  AbstractNVM(ObjectData &o, HIL::CommandManager *m)
      : Object(o), commandManager(m) {}
  virtual ~AbstractNVM() {}

  /**
   * \brief Handle HIL::SubCommand
   *
   * This HIL::SubCommand must have valid ppn field.
   *
   * \param[in] tag HIL::Command tag
   * \param[in] id  HIL::SubCommand id
   */
  virtual void enqueue(uint64_t tag, uint32_t id) = 0;
};

}  // namespace SimpleSSD::FIL::NVM

#endif
