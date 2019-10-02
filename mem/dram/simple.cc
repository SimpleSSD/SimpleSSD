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

SimpleDRAM::SimpleDRAM(ObjectData &&o) : AbstractDRAM(std::move(o)) {
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

}  // namespace SimpleSSD::Memory::DRAM
