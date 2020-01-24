// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "mem/dram/simple/simple.hh"

namespace SimpleSSD::Memory::DRAM {

Simple::Simple(ObjectData &o) : AbstractDRAM(o) {}

Simple::~Simple() {}

void Simple::getStatList(std::vector<Stat> &list, std::string prefix) noexcept {
  AbstractDRAM::getStatList(list, prefix);
}

void Simple::getStatValues(std::vector<double> &values) noexcept {
  AbstractDRAM::getStatValues(values);
}

void Simple::resetStatValues() noexcept {
  AbstractDRAM::resetStatValues();
}

void Simple::createCheckpoint(std::ostream &out) const noexcept {
  AbstractDRAM::createCheckpoint(out);
}

void Simple::restoreCheckpoint(std::istream &in) noexcept {
  AbstractDRAM::restoreCheckpoint(in);
}

}  // namespace SimpleSSD::Memory::DRAM
