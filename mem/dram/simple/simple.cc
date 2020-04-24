// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "mem/dram/simple/simple.hh"

namespace SimpleSSD::Memory::DRAM {

SimpleDRAM::SimpleDRAM(ObjectData &o) : AbstractDRAM(o), lastResetAt(0) {
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

  uint32_t totalBanks =
      pStructure->channel * pStructure->rank * pStructure->bank;

  rowOpened =
      std::vector<uint32_t>(totalBanks, std::numeric_limits<uint32_t>::max());

  scheduler.reserve(totalBanks);

  for (uint32_t i = 0; i < totalBanks; i++) {
    std::string name = "Memory::DRAM::scheduler<";

    name += std::to_string(i / (pStructure->bank * pStructure->rank));
    name += ":";
    name += std::to_string((i / pStructure->bank) % pStructure->rank);
    name += ":";
    name += std::to_string(i % pStructure->bank);
    name += ">";

    scheduler.emplace_back(new RequestScheduler(
        object, name, [this](Request *r) -> uint64_t { return preSubmit(r); },
        [this](Request *r) { postDone(r); }, Request::backup,
        Request::restore));
  }

  readHit.resize(totalBanks);
  writeHit.resize(totalBanks);

  // gem5 src/mem/drampower.cc
  spec.memArchSpec.burstLength = pStructure->burstLength;
  spec.memArchSpec.nbrOfBanks = pStructure->bank;
  spec.memArchSpec.nbrOfRanks = pStructure->rank;
  spec.memArchSpec.dataRate = 2;
  spec.memArchSpec.nbrOfColumns = 0;
  spec.memArchSpec.nbrOfRows = 0;
  spec.memArchSpec.width = pStructure->width;
  spec.memArchSpec.nbrOfBankGroups = 0;
  spec.memArchSpec.dll = false;
  spec.memArchSpec.twoVoltageDomains = pPower->pVDD[1] != 0.f;
  spec.memArchSpec.termination = false;

  spec.memTimingSpec.RC = DIVCEIL(pTiming->tRRD, pTiming->tCK);
  spec.memTimingSpec.RCD = DIVCEIL(pTiming->tRCD, pTiming->tCK);
  spec.memTimingSpec.RL = DIVCEIL(pTiming->tRL, pTiming->tCK);
  spec.memTimingSpec.RP = DIVCEIL(pTiming->tRP, pTiming->tCK);
  spec.memTimingSpec.RFC = DIVCEIL(pTiming->tRFC, pTiming->tCK);
  spec.memTimingSpec.RAS = spec.memTimingSpec.RRD - spec.memTimingSpec.RP;
  spec.memTimingSpec.WL = DIVCEIL(pTiming->tWL, pTiming->tCK);
  spec.memTimingSpec.DQSCK = DIVCEIL(pTiming->tDQSCK, pTiming->tCK);
  spec.memTimingSpec.RTP = DIVCEIL(pTiming->tRTP, pTiming->tCK);
  spec.memTimingSpec.WR = DIVCEIL(pTiming->tWR, pTiming->tCK);
  spec.memTimingSpec.XS = DIVCEIL(pTiming->tSR, pTiming->tCK);
  spec.memTimingSpec.clkPeriod = pTiming->tCK / 1000.;

  if (spec.memTimingSpec.clkPeriod == 0.) {
    panic("Invalid DRAM clock period");
  }

  spec.memTimingSpec.clkMhz = (1 / spec.memTimingSpec.clkPeriod) * 1000.;

  spec.memPowerSpec.idd0 = pPower->pIDD0[0];
  spec.memPowerSpec.idd02 = pPower->pIDD0[1];
  spec.memPowerSpec.idd2p0 = pPower->pIDD2P0[0];
  spec.memPowerSpec.idd2p02 = pPower->pIDD2P0[1];
  spec.memPowerSpec.idd2p1 = pPower->pIDD2P1[0];
  spec.memPowerSpec.idd2p12 = pPower->pIDD2P1[1];
  spec.memPowerSpec.idd2n = pPower->pIDD2N[0];
  spec.memPowerSpec.idd2n2 = pPower->pIDD2N[1];
  spec.memPowerSpec.idd3p0 = pPower->pIDD3P0[0];
  spec.memPowerSpec.idd3p02 = pPower->pIDD3P0[1];
  spec.memPowerSpec.idd3p1 = pPower->pIDD3P1[0];
  spec.memPowerSpec.idd3p12 = pPower->pIDD3P1[1];
  spec.memPowerSpec.idd3n = pPower->pIDD3N[0];
  spec.memPowerSpec.idd3n2 = pPower->pIDD3N[1];
  spec.memPowerSpec.idd4r = pPower->pIDD4R[0];
  spec.memPowerSpec.idd4r2 = pPower->pIDD4R[1];
  spec.memPowerSpec.idd4w = pPower->pIDD4W[0];
  spec.memPowerSpec.idd4w2 = pPower->pIDD4W[1];
  spec.memPowerSpec.idd5 = pPower->pIDD5[0];
  spec.memPowerSpec.idd52 = pPower->pIDD5[1];
  spec.memPowerSpec.idd6 = pPower->pIDD6[0];
  spec.memPowerSpec.idd62 = pPower->pIDD6[1];
  spec.memPowerSpec.vdd = pPower->pVDD[0];
  spec.memPowerSpec.vdd2 = pPower->pVDD[1];

  uint32_t totalRanks = pStructure->channel * pStructure->rank;

  for (uint8_t i = 0; i < totalRanks; i++) {
    auto &ret = dramPower.emplace_back(libDRAMPower(spec, false));

    ret.doCommand(Data::MemCommand::PREA, 0, 0);
  }

  powerStat.resize(totalRanks);
}

SimpleDRAM::~SimpleDRAM() {
  for (auto &iter : scheduler) {
    delete iter;
  }
}

bool SimpleDRAM::checkRow(Request *req) {
  auto &row = rowOpened.at(getBankOffset(req->address));

  if (row == req->address.row) {
    return true;
  }
  else {
    row = req->address.row;
  }

  return false;
}

uint64_t SimpleDRAM::preSubmit(Request *req) {
  // Calculate latency
  uint64_t burstCount = DIVCEIL(MemoryPacketSize, burstSize);
  uint64_t latency = burstCount * burstLatency;
  uint64_t now = getTick();

  uint32_t bankid = getBankOffset(req->address);

  auto &power = dramPower.at(getRankOffset(req->address));

  if (checkRow(req)) {
    if (req->read) {
      readHit.at(bankid).addHit(burstCount);
      power.doCommand(Data::MemCommand::RD, req->address.bank,
                      DIVCEIL(now, pTiming->tCK));
    }
    else {
      writeHit.at(bankid).addHit(burstCount);
      power.doCommand(Data::MemCommand::WR, req->address.bank,
                      DIVCEIL(now, pTiming->tCK));
    }
  }
  else {
    power.doCommand(Data::MemCommand::PRE, req->address.bank,
                    DIVCEIL(now, pTiming->tCK));
    power.doCommand(Data::MemCommand::ACT, req->address.bank,
                    DIVCEIL(now + pTiming->tRP, pTiming->tCK));

    if (req->read) {
      latency +=
          ioReadLatency + DIVCEIL(burstCount, burstInRow) * activateLatency;
      readHit.at(bankid).addMiss(burstCount);
      power.doCommand(Data::MemCommand::RD, req->address.bank,
                      DIVCEIL(now + activateLatency, pTiming->tCK));
    }
    else {
      latency +=
          ioWriteLatency + DIVCEIL(burstCount, burstInRow) * activateLatency;
      writeHit.at(bankid).addMiss(burstCount);
      power.doCommand(Data::MemCommand::WR, req->address.bank,
                      DIVCEIL(now + activateLatency, pTiming->tCK));
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
  auto req = new Request(true, eid, data);

  req->address = decodeAddress(address);
  req->beginAt = getTick();

  // Stat Update
  readStat.add(MemoryPacketSize);
  readBusy.busyBegin(req->beginAt);
  totalBusy.busyBegin(req->beginAt);

  // Schedule callback
  scheduler.at(getBankOffset(req->address))->enqueue(req);
}

void SimpleDRAM::write(uint64_t address, Event eid, uint64_t data) {
  auto req = new Request(false, eid, data);

  req->address = decodeAddress(address);
  req->beginAt = getTick();

  // Stat Update
  writeStat.add(MemoryPacketSize);
  writeBusy.busyBegin(req->beginAt);
  totalBusy.busyBegin(req->beginAt);

  // Schedule callback
  scheduler.at(getBankOffset(req->address))->enqueue(req);
}

void SimpleDRAM::getStatList(std::vector<Stat> &list,
                             std::string prefix) noexcept {
  AbstractDRAM::getStatList(list, prefix);

  uint32_t totalBanks =
      pStructure->channel * pStructure->rank * pStructure->bank;

  for (uint32_t i = 0; i < totalBanks; i++) {
    std::string name = "channel";

    name += std::to_string(i / (pStructure->bank * pStructure->rank));
    name += ".rank";
    name += std::to_string((i / pStructure->bank) % pStructure->rank);
    name += ".bank";
    name += std::to_string(i % pStructure->bank);
    name += ".";

    list.emplace_back(prefix + name + "read", "# read");
    list.emplace_back(prefix + name + "read.rowhit", "# rowhit while reading");
    list.emplace_back(prefix + name + "write", "# write");
    list.emplace_back(prefix + name + "write.rowhit", "# rowhit while writing");
  }

  list.emplace_back(prefix + "busy.read", "Total read busy ticks");
  list.emplace_back(prefix + "busy.write", "Total write busy ticks");
  list.emplace_back(prefix + "busy.total", "Total busy ticks");

  uint32_t totalRanks = pStructure->channel * pStructure->rank;

  for (uint32_t i = 0; i < totalRanks; i++) {
    std::string name = "channel";

    name += std::to_string(i / pStructure->rank);
    name += ".rank";
    name += std::to_string(i % pStructure->rank);
    name += ".";

    list.emplace_back(prefix + name + "energy.activate",
                      "Energy for activate commands per rank (pJ)");
    list.emplace_back(prefix + name + "energy.precharge",
                      "Energy for precharge commands per rank (pJ)");
    list.emplace_back(prefix + name + "energy.read",
                      "Energy for read commands per rank (pJ)");
    list.emplace_back(prefix + name + "energy.write",
                      "Energy for write commands per rank (pJ)");
    list.emplace_back(prefix + name + "energy.refresh",
                      "Energy for refresh commands per rank (pJ)");
    list.emplace_back(prefix + name + "energy.self_refresh",
                      "Energy for self refresh per rank (pJ)");
    list.emplace_back(prefix + name + "energy.total",
                      "Total energy per rank (pJ)");
    list.emplace_back(prefix + name + "power", "Core power per rank (mW)");
  }
}

void SimpleDRAM::getStatValues(std::vector<double> &values) noexcept {
  AbstractDRAM::getStatValues(values);

  uint64_t now = getTick();

  uint32_t totalBanks = readHit.size();

  for (uint32_t i = 0; i < totalBanks; i++) {
    values.push_back((double)readHit.at(i).getTotalCount());
    values.push_back((double)readHit.at(i).getHitCount());
    values.push_back((double)writeHit.at(i).getTotalCount());
    values.push_back((double)writeHit.at(i).getHitCount());
  }

  values.push_back((double)readBusy.getBusyTick(now));
  values.push_back((double)writeBusy.getBusyTick(now));
  values.push_back((double)totalBusy.getBusyTick(now));

  uint32_t totalRanks = pStructure->channel * pStructure->rank;

  for (uint32_t rank = 0; rank < totalRanks; rank++) {
    auto &power = dramPower.at(rank);
    auto &stat = powerStat.at(rank);

    power.calcWindowEnergy(getTick() / pTiming->tCK);

    auto &energy = power.getEnergy();

    stat.act_energy += energy.act_energy * pStructure->chip;
    stat.pre_energy += energy.pre_energy * pStructure->chip;
    stat.read_energy += energy.read_energy * pStructure->chip;
    stat.write_energy += energy.write_energy * pStructure->chip;
    stat.ref_energy += energy.ref_energy * pStructure->chip;
    stat.sref_energy += energy.sref_energy * pStructure->chip;
    stat.window_energy += energy.window_energy * pStructure->chip;

    values.push_back(stat.act_energy);
    values.push_back(stat.pre_energy);
    values.push_back(stat.read_energy);
    values.push_back(stat.write_energy);
    values.push_back(stat.ref_energy);
    values.push_back(stat.sref_energy);
    values.push_back(stat.window_energy);
    values.push_back(1000.0 * stat.window_energy / (getTick() - lastResetAt));
  }
}

void SimpleDRAM::resetStatValues() noexcept {
  AbstractDRAM::resetStatValues();

  uint64_t now = getTick();

  uint32_t totalBanks = readHit.size();

  for (uint32_t i = 0; i < totalBanks; i++) {
    readHit.at(i).clear();
    writeHit.at(i).clear();
  }

  readBusy.clear(now);
  writeBusy.clear(now);
  totalBusy.clear(now);

  for (auto &power : dramPower) {
    power.calcWindowEnergy(getTick() / pTiming->tCK);
  }

  for (auto &stat : powerStat) {
    stat.clear();
  }

  lastResetAt = getTick();
}

void SimpleDRAM::createCheckpoint(std::ostream &out) const noexcept {
  AbstractDRAM::createCheckpoint(out);

  BACKUP_SCALAR(out, activateLatency);
  BACKUP_SCALAR(out, ioReadLatency);
  BACKUP_SCALAR(out, ioWriteLatency);
  BACKUP_SCALAR(out, burstLatency);
  BACKUP_SCALAR(out, burstSize);
  BACKUP_SCALAR(out, burstInRow);

  uint32_t size = (uint32_t)scheduler.size();

  BACKUP_SCALAR(out, size);

  for (auto &iter : scheduler) {
    iter->createCheckpoint(out);
  }

  BACKUP_BLOB(out, rowOpened.data(), size * sizeof(uint32_t));
}

void SimpleDRAM::restoreCheckpoint(std::istream &in) noexcept {
  AbstractDRAM::restoreCheckpoint(in);

  RESTORE_SCALAR(in, activateLatency);
  RESTORE_SCALAR(in, ioReadLatency);
  RESTORE_SCALAR(in, ioWriteLatency);
  RESTORE_SCALAR(in, burstLatency);
  RESTORE_SCALAR(in, burstSize);
  RESTORE_SCALAR(in, burstInRow);

  uint32_t size;

  RESTORE_SCALAR(in, size);

  panic_if(size != scheduler.size(), "DRAM structure mismatch.");

  for (auto &iter : scheduler) {
    iter->restoreCheckpoint(in);
  }

  RESTORE_BLOB(in, rowOpened.data(), size * sizeof(uint32_t));
}

}  // namespace SimpleSSD::Memory::DRAM
