// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "mem/dram/lpddr4.hh"

namespace SimpleSSD::Memory::DRAM {

LPDDR4::Bank::Bank()
    : state(BankState::Precharge),
      row(std::numeric_limits<uint32_t>::max()),
      lastACT(0),
      lastPRE(0),
      lastREAD(0),
      lastWRITE(0) {}

LPDDR4::Rank::Rank()
    : activeBank(0),
      readRowHit(0),
      readCount(0),
      writeRowHit(0),
      writeCount(0) {}

LPDDR4::LPDDR4(ObjectData &o) : AbstractDRAM(o) {
  // Create ranks
  ranks.resize(pStructure->rank);

  uint8_t idx = 0;

  for (auto &rank : ranks) {
    // Link power
    rank.power = &dramPower.at(idx);

    // Create banks
    rank.banks.reserve(pStructure->bank);

    // Precharge
    for (auto &bank : rank.banks) {
      bank.state = BankState::Precharge;
    }

    // Precharge!
    rank.power->doCommand(Data::MemCommand::PREA, 0, 0);
  }

  // Shortcuts
  wtr = pTiming->tWL + (pStructure->burstChop / 2 + 1) * pTiming->tCK +
        pTiming->tWTR;
  rtw = pTiming->tRL + DIVCEIL(pTiming->tDQSCK, pTiming->tCK) +
        (pStructure->burstChop / 2) * pTiming->tCK - pTiming->tWL;
  wtw = (pStructure->burstChop / 2) * pTiming->tCK;
  rtr = wtw;

  panic_if(rtw > pTiming->tRL + pTiming->tWL, "Invalid timing.");

  // Make event
  eventCompletion =
      createEvent([this](uint64_t t, uint64_t) { completeRequest(t); },
                  "Memory::DRAM::LPDDR4::eventCompletion");
}

LPDDR4::~LPDDR4() {}

bool LPDDR4::isIdle(uint32_t, uint8_t) {
  return true;
}

uint32_t LPDDR4::getRowInfo(uint32_t rank, uint8_t bank) {
  panic_if(rank >= pStructure->rank, "Rank out of range.");
  panic_if(bank >= pStructure->bank, "Bank out of range.");

  return ranks[rank].banks[bank].row;
}

void LPDDR4::submit(Address address, uint32_t size, bool read, Event eid,
                    uint64_t data) {
  panic_if(address.rank >= pStructure->rank, "Rank out of range.");
  panic_if(address.bank >= pStructure->bank, "Bank out of range.");

  uint64_t now = getTick();
  uint64_t endAt = now;

  // Check bank state
  auto &rank = ranks[address.rank];
  auto &bank = rank.banks[address.bank];

  if (bank.state == BankState::Precharge) {
    bank.row = address.row;
    bank.state = BankState::Activate;
    bank.lastACT = MAX(bank.lastPRE + pTiming->tRP, now);

    rank.power->doCommand(Data::MemCommand::ACT, address.bank,
                          DIVCEIL(bank.lastACT, pTiming->tCK));
  }
  else {
    if (bank.row != address.row) {
      bank.row = address.row;

      // PRECHARGE ROW
      bank.lastPRE =
          MAX(bank.lastACT + pTiming->tRP,
              MAX(bank.lastREAD + 8 * pTiming->tCK + pTiming->tRTP,
                  MAX(bank.lastWRITE + pTiming->tWL +
                          pTiming->tCK * (pStructure->burstChop / 2 + 1) +
                          pTiming->tWR,
                      now)));

      rank.power->doCommand(Data::MemCommand::PRE, address.bank,
                            DIVCEIL(bank.lastPRE, pTiming->tCK));

      // ACTIVATE ROW
      bank.lastACT = bank.lastPRE + pTiming->tRP;

      rank.power->doCommand(Data::MemCommand::ACT, address.bank,
                            DIVCEIL(bank.lastACT, pTiming->tCK));
    }
    else {
      // Row hit
      if (read) {
        rank.readRowHit++;
      }
      else {
        rank.writeRowHit++;
      }
    }
  }

  if (read) {
    bank.lastREAD =
        MAX(bank.lastACT + pTiming->tRCD,
            MAX(bank.lastREAD + rtr, MAX(bank.lastWRITE + wtr, now)));

    rank.power->doCommand(Data::MemCommand::RD, address.bank,
                          DIVCEIL(bank.lastREAD, pTiming->tCK));

    readStat.count++;
    readStat.size += size;
    rank.readCount++;

    endAt = bank.lastREAD + pTiming->tRL + pTiming->tDQSCK +
            (pStructure->burstChop / 2) * pTiming->tCK;
  }
  else {
    bank.lastWRITE =
        MAX(bank.lastACT + pTiming->tRCD,
            MAX(bank.lastREAD + rtw, MAX(bank.lastWRITE + wtr, now)));

    rank.power->doCommand(Data::MemCommand::WR, address.bank,
                          DIVCEIL(bank.lastWRITE, pTiming->tCK));

    writeStat.count++;
    writeStat.size += size;
    rank.writeCount++;

    endAt = bank.lastREAD + pTiming->tWL +
            (pStructure->burstChop / 2 + 1) * pTiming->tCK;
  }

  // Push to queue
  requestQueue.emplace(endAt, Entry(eid, data));

  reschedule();
}

void LPDDR4::completeRequest(uint64_t now) {
  auto iter = requestQueue.begin();

  panic_if(iter == requestQueue.end(), "Unexpected end of queue.");
  panic_if(iter->first != now, "Queue corrupted.");

  scheduleNow(iter->second.eid, iter->second.data);

  requestQueue.erase(iter);

  reschedule();
}

void LPDDR4::reschedule() {
  if (isScheduled(eventCompletion)) {
    if (object.cpu->when(eventCompletion) != requestQueue.begin()->first) {
      deschedule(eventCompletion);
    }
  }

  scheduleAbs(eventCompletion, 0, requestQueue.begin()->first);
}

void LPDDR4::getStatList(std::vector<Stat> &list, std::string prefix) noexcept {
  AbstractDRAM::getStatList(list, prefix);

  for (uint8_t rank = 0; rank < pStructure->rank; rank++) {
    std::string rprefix = prefix + "rank" + std::to_string(rank) + ".";

    list.emplace_back(rprefix + "request_count.read", "Read request count.");
    list.emplace_back(rprefix + "request_count.write", "Write request count.");
    list.emplace_back(rprefix + "rowhit.count.read", "Read row hit count.");
    list.emplace_back(rprefix + "rowhit.count.write", "Write row hit count.");
    list.emplace_back(rprefix + "rowhit.ratio.read", "Read row hit ratio.");
    list.emplace_back(rprefix + "rowhit.ratio.write", "Write row hit ratio.");
  }
}

void LPDDR4::getStatValues(std::vector<double> &values) noexcept {
  AbstractDRAM::getStatValues(values);

  for (auto &rank : ranks) {
    values.push_back((double)rank.readCount);
    values.push_back((double)rank.writeCount);
    values.push_back((double)rank.readRowHit);
    values.push_back((double)rank.writeRowHit);
    values.push_back((double)rank.readRowHit / rank.readCount);
    values.push_back((double)rank.writeRowHit / rank.writeCount);
  }
}

void LPDDR4::resetStatValues() noexcept {
  AbstractDRAM::resetStatValues();

  for (auto &rank : ranks) {
    rank.readCount = 0;
    rank.writeCount = 0;
    rank.readRowHit = 0;
    rank.writeRowHit = 0;
  }
}

void LPDDR4::createCheckpoint(std::ostream &) const noexcept {}

void LPDDR4::restoreCheckpoint(std::istream &) noexcept {}

}  // namespace SimpleSSD::Memory::DRAM
