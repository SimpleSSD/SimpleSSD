// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "mem/dram/simple/rank.hh"

#include "mem/dram/simple/controller.hh"

namespace SimpleSSD::Memory::DRAM::Simple {

Rank::Rank(ObjectData &o, Controller *c, Timing *t)
    : Object(o), parent(c), timing(t), pendingRefresh(false) {
  // Create bank
  auto dram = object.config->getDRAM();

  banks.reserve(dram->bank);

  for (uint8_t i = 0; i < dram->bank; i++) {
    banks.emplace_back(Bank(object, this, timing));
  }
}

Rank::~Rank() {}

bool Rank::submit(Packet *pkt) {
  auto &bank = banks[pkt->bank];

  return bank.submit(pkt);
}

void Rank::powerEvent() {}

void Rank::completion(uint64_t id) {
  // parent->completion(id);
}

void Rank::getStatList(std::vector<Stat> &list, std::string prefix) noexcept {
  list.emplace_back(prefix + "read", "Read command count");
  list.emplace_back(prefix + "write", "Write command count");

  uint8_t bid = 0;

  for (auto &iter : banks) {
    std::string bprefix = prefix + "bank" + std::to_string(bid++) + '.';

    iter.getStatList(list, bprefix);
  }
}

void Rank::getStatValues(std::vector<double> &values) noexcept {
  values.push_back((double)readStat.getCount());
  values.push_back((double)writeStat.getCount());

  for (auto &iter : banks) {
    iter.getStatValues(values);
  }
}

void Rank::resetStatValues() noexcept {
  readStat.clear();
  writeStat.clear();

  for (auto &iter : banks) {
    iter.resetStatValues();
  }
}

void Rank::createCheckpoint(std::ostream &out) const noexcept {
  BACKUP_SCALAR(out, pendingRefresh);

  for (auto &iter : banks) {
    iter.createCheckpoint(out);
  }

  readStat.createCheckpoint(out);
  writeStat.createCheckpoint(out);
}

void Rank::restoreCheckpoint(std::istream &in) noexcept {
  RESTORE_SCALAR(in, pendingRefresh);

  for (auto &iter : banks) {
    iter.restoreCheckpoint(in);
  }

  readStat.restoreCheckpoint(in);
  writeStat.restoreCheckpoint(in);
}

}  // namespace SimpleSSD::Memory::DRAM::Simple
