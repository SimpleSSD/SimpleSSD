// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "mem/dram/simple/simple.hh"

namespace SimpleSSD::Memory::DRAM::Simple {

SimpleDRAM::SimpleDRAM(ObjectData &o)
    : AbstractDRAM(o),
      scheduler(
          o, "Memory::DRAM::scheduler",
          [this](Request *r) -> uint64_t { return preSubmit(r); },
          [this](Request *r) { postDone(r); }, Request::backup,
          Request::restore) {
  activateLatency = pTiming->tRP + pTiming->tRCD;
  ioReadLatency = pTiming->tRL;
  ioWriteLatency = pTiming->tWL + pTiming->tWR;
  burstSize = pStructure->width * pStructure->chip * pStructure->channel / 8;
  burstSize *= pStructure->burstLength;
  burstLatency = pTiming->tCK * burstSize / 2;
  burstInRow = pStructure->rowSize / burstSize;

  addressLimit.channel = pStructure->channel;
  addressLimit.rank = pStructure->rank;
  addressLimit.bank = pStructure->bank;
  addressLimit.row =
      pStructure->chipSize / pStructure->bank / pStructure->rowSize;

  decodeAddress = [channel = addressLimit.channel, rank = addressLimit.rank,
                   bank = addressLimit.bank, row = addressLimit.row,
                   col = pStructure->rowSize](uint64_t addr) -> Address {
    Address ret;

    addr = addr / col;
    ret.channel = addr % channel;
    addr = addr / channel;
    ret.bank = addr % bank;
    addr = addr / bank;
    ret.rank = addr % rank;
    addr = addr / rank;
    ret.row = addr % row;

    return ret;
  };

  rowOpened.resize(pStructure->channel);

  for (auto &channel : rowOpened) {
    channel.resize(pStructure->rank);

    for (auto &rank : channel) {
      rank.resize(pStructure->bank);

      for (auto &bank : rank) {
        bank = std::numeric_limits<uint32_t>::max();
      }
    }
  }

  readHit.clear();
  writeHit.clear();
}

SimpleDRAM::~SimpleDRAM() {}

bool SimpleDRAM::checkRow(uint64_t addr) {
  auto dram = decodeAddress(addr);
  auto &row = rowOpened[dram.channel][dram.rank][dram.bank];

  if (row == dram.row) {
    return true;
  }
  else {
    row = dram.row;
  }

  return false;
}

uint64_t SimpleDRAM::preSubmit(Request *req) {
  // Calculate latency
  uint64_t burstCount = DIVCEIL(MemoryPacketSize, burstSize);
  uint64_t latency = burstCount * burstLatency;

  if (checkRow(req->offset)) {
    if (req->read) {
      readHit.add(burstCount);
    }
    else {
      writeHit.add(burstCount);
    }
  }
  else {
    if (req->read) {
      latency +=
          ioReadLatency + DIVCEIL(burstCount, burstInRow) * activateLatency;
    }
    else {
      latency +=
          ioWriteLatency + DIVCEIL(burstCount, burstInRow) * activateLatency;
    }
  }

  return latency;
}

void SimpleDRAM::postDone(Request *req) {
  // Calculate latency
  uint64_t now = getTick();

  if (req->read) {
    readBusy.busyEnd(now);
  }
  else {
    writeBusy.busyEnd(now);
  }

  totalBusy.busyEnd(now);

  scheduleNow(req->eid, req->data);

  delete req;
}

void SimpleDRAM::read(uint64_t address, Event eid, uint64_t data) {
  auto req = new Request(true, address, eid, data);

  req->beginAt = getTick();

  // Stat Update
  writeStat.add(MemoryPacketSize);
  writeBusy.busyBegin(req->beginAt);
  totalBusy.busyBegin(req->beginAt);

  // Schedule callback
  scheduler.enqueue(req);
}

void SimpleDRAM::write(uint64_t address, Event eid, uint64_t data) {
  auto req = new Request(false, address, eid, data);

  req->beginAt = getTick();

  // Stat Update
  writeStat.add(MemoryPacketSize);
  writeBusy.busyBegin(req->beginAt);
  totalBusy.busyBegin(req->beginAt);

  // Schedule callback
  scheduler.enqueue(req);
}

void SimpleDRAM::getStatList(std::vector<Stat> &list,
                             std::string prefix) noexcept {
  AbstractDRAM::getStatList(list, prefix);

  list.emplace_back(prefix + "rowhit.read", "# rowhit while reading");
  list.emplace_back(prefix + "rowhit.write", "# rowhit while writing");
  list.emplace_back(prefix + "busy.read", "Total read busy ticks");
  list.emplace_back(prefix + "busy.write", "Total write busy ticks");
  list.emplace_back(prefix + "busy.total", "Total busy ticks");
}

void SimpleDRAM::getStatValues(std::vector<double> &values) noexcept {
  AbstractDRAM::getStatValues(values);

  uint64_t now = getTick();

  values.push_back((double)readHit.getCount());
  values.push_back((double)writeHit.getCount());
  values.push_back((double)readBusy.getBusyTick(now));
  values.push_back((double)writeBusy.getBusyTick(now));
  values.push_back((double)totalBusy.getBusyTick(now));
}

void SimpleDRAM::resetStatValues() noexcept {
  AbstractDRAM::resetStatValues();

  uint64_t now = getTick();

  readHit.clear();
  writeHit.clear();
  readBusy.clear(now);
  writeBusy.clear(now);
  totalBusy.clear(now);
}

void SimpleDRAM::createCheckpoint(std::ostream &out) const noexcept {
  AbstractDRAM::createCheckpoint(out);

  BACKUP_SCALAR(out, activateLatency);
  BACKUP_SCALAR(out, ioReadLatency);
  BACKUP_SCALAR(out, ioWriteLatency);
  BACKUP_SCALAR(out, burstLatency);
  BACKUP_SCALAR(out, burstSize);
  BACKUP_SCALAR(out, burstInRow);

  scheduler.createCheckpoint(out);
}

void SimpleDRAM::restoreCheckpoint(std::istream &in) noexcept {
  AbstractDRAM::restoreCheckpoint(in);

  RESTORE_SCALAR(in, activateLatency);
  RESTORE_SCALAR(in, ioReadLatency);
  RESTORE_SCALAR(in, ioWriteLatency);
  RESTORE_SCALAR(in, burstLatency);
  RESTORE_SCALAR(in, burstSize);
  RESTORE_SCALAR(in, burstInRow);

  scheduler.restoreCheckpoint(in);
}

}  // namespace SimpleSSD::Memory::DRAM::Simple
