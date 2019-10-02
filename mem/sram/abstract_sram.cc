// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "mem/sram/abstract_sram.hh"

namespace SimpleSSD::Memory::SRAM {

AbstractSRAM::AbstractSRAM(ObjectData &o, std::string prefix)
    : AbstractFIFO(o, prefix) {
  pStructure = config->getSRAM();
}

AbstractSRAM::~AbstractSRAM() {}

void AbstractSRAM::rangeCheck(uint64_t address, uint64_t length) noexcept {
  panic_if(address >= pStructure->size, "Address (0x%" PRIx64 ") out of range!",
           address);
  panic_if(address + length >= pStructure->size,
           "Address + Length (0x%" PRIx64 ") out of range!", address + length);
}

void AbstractSRAM::getStatList(std::vector<Stat> &list, std::string prefix) {
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

void AbstractSRAM::getStatValues(std::vector<double> &values) {
  values.push_back(readStat.count);
  values.push_back(readStat.size);
  values.push_back(writeStat.count);
  values.push_back(writeStat.size);
  values.push_back(readStat.count + writeStat.count);
  values.push_back(readStat.size + writeStat.size);
}

void AbstractSRAM::resetStatValues() {
  readStat.clear();
  writeStat.clear();
}

}  // namespace SimpleSSD::Memory::SRAM
