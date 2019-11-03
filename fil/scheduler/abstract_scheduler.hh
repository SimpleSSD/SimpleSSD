// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_FIL_SCHEDULER_ABSTRACT_SCHEDULER_HH__
#define __SIMPLESSD_FIL_SCHEDULER_ABSTRACT_SCHEDULER_HH__

#include <cinttypes>

#include "hil/command_manager.hh"
#include "sim/object.hh"

namespace SimpleSSD::FIL::Scheduler {

class AbstractScheduler : public Object {
 protected:
  CommandManager *commandManager;

 public:
  AbstractScheduler(ObjectData &o, CommandManager *m)
      : Object(o), commandManager(m) {}
  virtual ~AbstractScheduler() {}

  virtual void enqueue(uint64_t tag) = 0;
};

}  // namespace SimpleSSD::FIL::Scheduler

#endif
