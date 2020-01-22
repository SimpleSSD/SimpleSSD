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
}

void AbstractSRAM::getStatValues(std::vector<double> &values) noexcept {
  values.push_back((double)readStat.count);
  values.push_back((double)readStat.size);
  values.push_back((double)writeStat.count);
  values.push_back((double)writeStat.size);
  values.push_back((double)(readStat.count + writeStat.count));
  values.push_back((double)(readStat.size + writeStat.size));
}

void AbstractSRAM::resetStatValues() noexcept {
  readStat.clear();
  writeStat.clear();
}

void AbstractSRAM::createCheckpoint(std::ostream &out) const noexcept {
  BACKUP_SCALAR(out, readStat);
  BACKUP_SCALAR(out, writeStat);
}

void AbstractSRAM::restoreCheckpoint(std::istream &in) noexcept {
  RESTORE_SCALAR(in, readStat);
  RESTORE_SCALAR(in, writeStat);
}

}  // namespace SimpleSSD::Memory::SRAM
