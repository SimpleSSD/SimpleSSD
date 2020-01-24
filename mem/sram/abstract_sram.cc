// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "mem/sram/abstract_sram.hh"

namespace SimpleSSD::Memory::SRAM {

AbstractSRAM::AbstractSRAM(ObjectData &o) : Object(o) {
  pStructure = o.config->getSRAM();

  totalEnergy = 0;
  averagePower = 0;
}

AbstractSRAM::~AbstractSRAM() {}

void AbstractSRAM::rangeCheck(uint64_t address, uint64_t length) noexcept {
  panic_if(address >= pStructure->size, "Address (0x%" PRIx64 ") out of range!",
           address);
  panic_if(address + length >= pStructure->size,
           "Address + Length (0x%" PRIx64 ") out of range!", address + length);
}

void AbstractSRAM::getStatList(std::vector<Stat> &list,
                               std::string prefix) noexcept {
  list.emplace_back(prefix + "read.request_count", "Read request count");
  list.emplace_back(prefix + "read.bytes", "Read data size in byte");
  list.emplace_back(prefix + "write.request_count", "Write request count");
  list.emplace_back(prefix + "write.bytes", "Write data size in byte");
  list.emplace_back(prefix + "request_count", "Total request count");
  list.emplace_back(prefix + "bytes", "Total data size in byte");
  list.emplace_back(prefix + "energy", "Total energy (pJ)");
  list.emplace_back(prefix + "power", "Average power (mW)");
}

void AbstractSRAM::getStatValues(std::vector<double> &values) noexcept {
  values.push_back((double)readStat.getCount());
  values.push_back((double)readStat.getSize());
  values.push_back((double)writeStat.getCount());
  values.push_back((double)writeStat.getSize());
  values.push_back((double)(readStat.getCount() + writeStat.getCount()));
  values.push_back((double)(readStat.getSize() + writeStat.getSize()));
  values.push_back(totalEnergy);
  values.push_back(averagePower);
}

void AbstractSRAM::resetStatValues() noexcept {
  readStat.clear();
  writeStat.clear();

  totalEnergy = 0;
  averagePower = 0;
}

void AbstractSRAM::createCheckpoint(std::ostream &out) const noexcept {
  readStat.createCheckpoint(out);
  writeStat.createCheckpoint(out);

  BACKUP_SCALAR(out, totalEnergy);
  BACKUP_SCALAR(out, averagePower);
}

void AbstractSRAM::restoreCheckpoint(std::istream &in) noexcept {
  readStat.restoreCheckpoint(in);
  writeStat.restoreCheckpoint(in);

  RESTORE_SCALAR(in, totalEnergy);
  RESTORE_SCALAR(in, averagePower);
}

}  // namespace SimpleSSD::Memory::SRAM
