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

SimpleDRAM::SimpleDRAM(ObjectData &o)
    : AbstractDRAM(o),
      scheduler(
          o, "Memory::SRAM::scheduler",
          [this](Request *r) -> uint64_t { return preSubmitRead(r); },
          [this](Request *r) -> uint64_t { return preSubmitWrite(r); },
          [this](Request *r) { postDone(r); },
          [this](Request *r) { postDone(r); }, Request::backup,
          Request::restore) {
  pageFetchLatency = pTiming->tRP + pTiming->tRAS;
  interfaceBandwidth = 2.0 * pStructure->width * pStructure->chip *
                       pStructure->channel / 8.0 / pTiming->tCK;
  unallocated = pStructure->chipSize * pStructure->chip * pStructure->rank *
                pStructure->channel;

  autoRefresh = createEvent(
      [this](uint64_t now, uint64_t) {
        dramPower->doCommand(Data::MemCommand::REF, 0, now / pTiming->tCK);

        schedule(autoRefresh, REFRESH_PERIOD);
      },
      "SimpleSSD::Memory::DRAM::SimpleDRAM::autoRefresh");

  schedule(autoRefresh, REFRESH_PERIOD);
}

SimpleDRAM::~SimpleDRAM() {
  // DO NOTHING
}

uint64_t SimpleDRAM::preSubmitRead(Request *req) {
  // Calculate latency
  uint64_t pageCount =
      (req->length > 0) ? (req->length - 1) / pStructure->pageSize + 1 : 0;
  uint64_t latency =
      (uint64_t)(pageCount * (pageFetchLatency +
                              pStructure->pageSize / interfaceBandwidth));

  req->beginAt = getTick() / pTiming->tCK;

  dramPower->doCommand(Data::MemCommand::ACT, 0, req->beginAt);

  for (uint64_t i = 0; i < pageCount; i++) {
    dramPower->doCommand(Data::MemCommand::RD, 0,
                         req->beginAt + spec.memTimingSpec.RCD);

    req->beginAt += spec.memTimingSpec.RCD;
  }

  req->beginAt -= spec.memTimingSpec.RCD;

  dramPower->doCommand(Data::MemCommand::PRE, 0,
                       req->beginAt + spec.memTimingSpec.RAS);

  return latency;
}

uint64_t SimpleDRAM::preSubmitWrite(Request *req) {
  // Calculate latency
  uint64_t pageCount =
      (req->length > 0) ? (req->length - 1) / pStructure->pageSize + 1 : 0;
  uint64_t latency =
      (uint64_t)(pageCount * (pageFetchLatency +
                              pStructure->pageSize / interfaceBandwidth));

  req->beginAt = getTick() / pTiming->tCK;

  dramPower->doCommand(Data::MemCommand::ACT, 0, req->beginAt);

  for (uint64_t i = 0; i < pageCount; i++) {
    dramPower->doCommand(Data::MemCommand::WR, 0,
                         req->beginAt + spec.memTimingSpec.RCD);

    req->beginAt += spec.memTimingSpec.RCD;
  }

  req->beginAt -= spec.memTimingSpec.RCD;

  dramPower->doCommand(Data::MemCommand::PRE, 0,
                       req->beginAt + spec.memTimingSpec.RAS);

  return latency;
}

void SimpleDRAM::postDone(Request *req) {
  updateStats(req->beginAt + spec.memTimingSpec.RAS + spec.memTimingSpec.RP);

  schedule(req->eid);

  delete req;
}

uint64_t SimpleDRAM::allocate(uint64_t size) {
  uint64_t ret = 0;

  for (auto &iter : addressMap) {
    unallocated -= iter.second;
  }

  panic_if(unallocated < size,
           "%" PRIu64 " bytes requested, but %" PRIu64 "bytes left in SRAM.",
           size, unallocated);

  if (addressMap.size() > 0) {
    ret = addressMap.back().first + addressMap.back().second;
  }

  addressMap.emplace_back(std::make_pair(ret, size));

  return ret;
}

void SimpleDRAM::read(uint64_t address, uint64_t length, Event eid) {
  auto req = new Request(address, length, eid);

  // Stat Update
  readStat.count++;
  readStat.size += length;

  // Schedule callback
  scheduler.read(req);
}

void SimpleDRAM::write(uint64_t address, uint64_t length, Event eid) {
  auto req = new Request(address, length, eid);

  // Stat Update
  writeStat.count++;
  writeStat.size += length;

  // Schedule callback
  scheduler.write(req);
}

void SimpleDRAM::createCheckpoint(std::ostream &out) const noexcept {
  AbstractDRAM::createCheckpoint(out);

  BACKUP_EVENT(out, autoRefresh);
  BACKUP_SCALAR(out, pageFetchLatency);
  BACKUP_SCALAR(out, unallocated);
  BACKUP_SCALAR(out, interfaceBandwidth);

  uint64_t size = addressMap.size();
  BACKUP_SCALAR(out, size);

  for (auto &iter : addressMap) {
    BACKUP_SCALAR(out, iter.first);
    BACKUP_SCALAR(out, iter.second);
  }

  scheduler.createCheckpoint(out);
}

void SimpleDRAM::restoreCheckpoint(std::istream &in) noexcept {
  AbstractDRAM::restoreCheckpoint(in);

  RESTORE_EVENT(in, autoRefresh);
  RESTORE_SCALAR(in, pageFetchLatency);
  RESTORE_SCALAR(in, unallocated);
  RESTORE_SCALAR(in, interfaceBandwidth);

  uint64_t size;
  RESTORE_SCALAR(in, size);

  for (uint64_t i = 0; i < size; i++) {
    uint64_t a, s;

    RESTORE_SCALAR(in, a);
    RESTORE_SCALAR(in, s);

    addressMap.push_back(std::make_pair(a, s));
  }

  scheduler.restoreCheckpoint(in);
}

}  // namespace SimpleSSD::Memory::DRAM
