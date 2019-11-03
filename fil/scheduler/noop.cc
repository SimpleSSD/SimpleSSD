// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "fil/scheduler/noop.hh"

namespace SimpleSSD::FIL::Scheduler {

Noop::Noop(ObjectData &o, CommandManager *m, NVM::AbstractNVM *n)
    : AbstractScheduler(o, m, n) {}

Noop::~Noop() {}

void Noop::enqueue(uint64_t tag) {
  auto &list = commandManager->getSubCommand(tag);

  panic_if(list.size() == 0, "Unexpected empty subcommands.");

  for (auto &scmd : list) {
    if (scmd.ppn == InvalidPPN) {
      continue;
    }

    pNVM->enqueue(tag, scmd.id);
  }
}

}  // namespace SimpleSSD::FIL::Scheduler
