// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_FIL_SCHEDULER_NOOP_HH__
#define __SIMPLESSD_FIL_SCHEDULER_NOOP_HH__

#include "fil/scheduler/abstract_scheduler.hh"

namespace SimpleSSD::FIL::Scheduler {

class Noop : public AbstractScheduler {
 public:
  Noop(ObjectData &, CommandManager *, NVM::AbstractNVM *);
  ~Noop();

  void enqueue(uint64_t tag) override;
};

}  // namespace SimpleSSD::FIL::Scheduler

#endif
