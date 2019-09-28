// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "mem/dram/simple.hh"

#include "util/algorithm.hh"

namespace SimpleSSD::Memory::DRAM {

#define REFRESH_PERIOD 64000000000

SimpleDRAM::SimpleDRAM(ObjectData &o) : AbstractDRAM(o) {
  pageFetchLatency = pTiming->tRP + pTiming->tRAS;
  interfaceBandwidth = 2.0 * pStructure->width * pStructure->chip *
                       pStructure->channel / 8.0 / pTiming->tCK;

  autoRefresh = createEvent([this](uint64_t now, void *) {
    dramPower->doCommand(Data::MemCommand::REF, 0, now / pTiming->tCK);

    schedule(autoRefresh, now + REFRESH_PERIOD);
  }, "SimpleSSD::Memory::DRAM::SimpleDRAM::autoRefresh");

  schedule(autoRefresh, getTick() + REFRESH_PERIOD);
}

SimpleDRAM::~SimpleDRAM() {
  // DO NOTHING
}

void SimpleDRAM::updateStats(uint64_t cycle) {
  dramPower->calcWindowEnergy(cycle);

  auto &energy = dramPower->getEnergy();
  auto &power = dramPower->getPower();

  totalEnergy += energy.window_energy;
  totalPower = power.average_power;
}

void SimpleDRAM::read(uint64_t, uint64_t length, Event eid, void *context) {
  uint64_t beginAt = getTick();
  uint64_t pageCount = (length > 0) ? (length - 1) / pStructure->pageSize + 1 : 0;
  uint64_t latency =
      (uint64_t)(pageCount * (pageFetchLatency +
                              pStructure->pageSize / interfaceBandwidth));

  // DRAMPower uses cycle unit
  beginAt /= pTiming->tCK;

  dramPower->doCommand(Data::MemCommand::ACT, 0, beginAt);

  for (uint64_t i = 0; i < pageCount; i++) {
    dramPower->doCommand(Data::MemCommand::RD, 0,
                         beginAt + spec.memTimingSpec.RCD);

    beginAt += spec.memTimingSpec.RCD;
  }

  beginAt -= spec.memTimingSpec.RCD;

  dramPower->doCommand(Data::MemCommand::PRE, 0,
                       beginAt + spec.memTimingSpec.RAS);

  // Stat Update
  updateStats(beginAt + spec.memTimingSpec.RAS + spec.memTimingSpec.RP);
  readStat.count++;
  readStat.size += length;

  // Schedule callback
  schedule(eid, beginAt + latency);
}

void SimpleDRAM::write(uint64_t, uint64_t length, Event eid, void *context) {
  uint64_t beginAt = getTick();
  uint64_t pageCount = (length > 0) ? (length - 1) / pStructure->pageSize + 1 : 0;
  uint64_t latency =
      (uint64_t)(pageCount * (pageFetchLatency +
                              pStructure->pageSize / interfaceBandwidth));

  // DRAMPower uses cycle unit
  beginAt /= pTiming->tCK;

  dramPower->doCommand(Data::MemCommand::ACT, 0, beginAt);

  for (uint64_t i = 0; i < pageCount; i++) {
    dramPower->doCommand(Data::MemCommand::WR, 0,
                         beginAt + spec.memTimingSpec.RCD);

    beginAt += spec.memTimingSpec.RCD;
  }

  beginAt -= spec.memTimingSpec.RCD;

  dramPower->doCommand(Data::MemCommand::PRE, 0,
                       beginAt + spec.memTimingSpec.RAS);

  // Stat Update
  updateStats(beginAt + spec.memTimingSpec.RAS + spec.memTimingSpec.RP);
  writeStat.count++;
  writeStat.size += length;

  // Schedule callback
  schedule(eid, beginAt + latency);
}

void SimpleDRAM::getStatList(std::vector<Stat> &list, std::string prefix) {
  Stat temp;

  AbstractDRAM::getStatList(list, prefix);

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

void SimpleDRAM::getStatValues(std::vector<double> &values) {
  AbstractDRAM::getStatValues(values);

  values.push_back(readStat.count);
  values.push_back(readStat.size);
  values.push_back(writeStat.count);
  values.push_back(writeStat.size);
  values.push_back(readStat.count + writeStat.count);
  values.push_back(readStat.size + writeStat.size);
}

void SimpleDRAM::resetStatValues() {
  AbstractDRAM::resetStatValues();

  readStat.clear();
  writeStat.clear();
}

}  // namespace SimpleSSD::Memory::DRAM
