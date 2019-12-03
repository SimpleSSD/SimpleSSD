// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "fil/scheduler/noop.hh"

namespace SimpleSSD::FIL::Scheduler {

Noop::Noop(ObjectData &o, NVM::AbstractNVM *n) : AbstractScheduler(o, n) {}

Noop::~Noop() {}

void Noop::submit(Request *req) {
  pNVM->submit(req);
}

void Noop::getStatList(std::vector<Stat> &, std::string) noexcept {}

void Noop::getStatValues(std::vector<double> &) noexcept {}

void Noop::resetStatValues() noexcept {}

void Noop::createCheckpoint(std::ostream &) const noexcept {}

void Noop::restoreCheckpoint(std::istream &) noexcept {}

}  // namespace SimpleSSD::FIL::Scheduler
