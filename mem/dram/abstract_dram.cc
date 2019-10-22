// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "mem/dram/abstract_dram.hh"

#include <cstring>

#include "util/algorithm.hh"

namespace SimpleSSD::Memory::DRAM {

AbstractDRAM::AbstractDRAM(ObjectData &o) : AbstractRAM(o) {
  pStructure = o.config->getDRAM();
  pTiming = o.config->getDRAMTiming();
  pPower = o.config->getDRAMPower();
}

AbstractDRAM::~AbstractDRAM() {}

void AbstractDRAM::getStatList(std::vector<Stat> &list,
                               std::string prefix) noexcept {
  Stat temp;

  temp.name = prefix + "read.request_count";
  temp.desc = "Read request count";
  list.push_back(temp);

  temp.name = prefix + "read.bytes";
  temp.desc = "Read data size in byte";
  list.push_back(temp);

  temp.name = prefix + "write.request_count";
  temp.desc = "Write request count";
  list.push_back(temp);

  temp.name = prefix + "write.bytes";
  temp.desc = "Write data size in byte";
  list.push_back(temp);

  temp.name = prefix + "request_count";
  temp.desc = "Total request count";
  list.push_back(temp);

  temp.name = prefix + "bytes";
  temp.desc = "Total data size in byte";
  list.push_back(temp);
}

void AbstractDRAM::getStatValues(std::vector<double> &values) noexcept {
  values.push_back(readStat.count);
  values.push_back(readStat.size);
  values.push_back(writeStat.count);
  values.push_back(writeStat.size);
  values.push_back(readStat.count + writeStat.count);
  values.push_back(readStat.size + writeStat.size);
}

void AbstractDRAM::resetStatValues() noexcept {
  readStat.clear();
  writeStat.clear();
}

void AbstractDRAM::createCheckpoint(std::ostream &out) const noexcept {
  BACKUP_SCALAR(out, readStat);
  BACKUP_SCALAR(out, writeStat);
}

void AbstractDRAM::restoreCheckpoint(std::istream &in) noexcept {
  RESTORE_SCALAR(in, readStat);
  RESTORE_SCALAR(in, writeStat);
}

}  // namespace SimpleSSD::Memory::DRAM
