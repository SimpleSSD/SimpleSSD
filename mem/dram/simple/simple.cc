// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "mem/dram/simple/simple.hh"

namespace SimpleSSD::Memory::DRAM::Simple {

SimpleDRAM::SimpleDRAM(ObjectData &o) : AbstractDRAM(o) {}

SimpleDRAM::~SimpleDRAM() {}

void SimpleDRAM::getStatList(std::vector<Stat> &list,
                             std::string prefix) noexcept {
  AbstractDRAM::getStatList(list, prefix);
}

void SimpleDRAM::getStatValues(std::vector<double> &values) noexcept {
  AbstractDRAM::getStatValues(values);
}

void SimpleDRAM::resetStatValues() noexcept {
  AbstractDRAM::resetStatValues();
}

void SimpleDRAM::createCheckpoint(std::ostream &out) const noexcept {
  AbstractDRAM::createCheckpoint(out);
}

void SimpleDRAM::restoreCheckpoint(std::istream &in) noexcept {
  AbstractDRAM::restoreCheckpoint(in);
}

}  // namespace SimpleSSD::Memory::DRAM::Simple
