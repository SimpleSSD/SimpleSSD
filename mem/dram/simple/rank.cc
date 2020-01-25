// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "mem/dram/simple/rank.hh"

namespace SimpleSSD::Memory::DRAM::Simple {

BankStatus::BankStatus()
    : state(BankState::Idle),
      activatedRowIndex(0),
      nextRead(0),
      nextWrite(0),
      nextActivate(0),
      nextPrecharge(0),
      nextPowerUp(0) {}

BankStatus::~BankStatus() {}

void BankStatus::createCheckpoint(std::ostream &out) const noexcept {
  BACKUP_SCALAR(out, state);
  BACKUP_SCALAR(out, activatedRowIndex);
  BACKUP_SCALAR(out, nextRead);
  BACKUP_SCALAR(out, nextWrite);
  BACKUP_SCALAR(out, nextActivate);
  BACKUP_SCALAR(out, nextPrecharge);
  BACKUP_SCALAR(out, nextPowerUp);
}

void BankStatus::restoreCheckpoint(std::istream &in) noexcept {
  RESTORE_SCALAR(in, state);
  RESTORE_SCALAR(in, activatedRowIndex);
  RESTORE_SCALAR(in, nextRead);
  RESTORE_SCALAR(in, nextWrite);
  RESTORE_SCALAR(in, nextActivate);
  RESTORE_SCALAR(in, nextPrecharge);
  RESTORE_SCALAR(in, nextPowerUp);
}

Rank::Rank(ObjectData &o)
    : Object(o), pendingRefresh(false) {
  // Create bank status
  auto dram = object.config->getDRAM();

  banks.resize(dram->bank);

  // Make timing shortcuts (JESD209-4)
  timing = object.config->getDRAMTiming();

  // Calculate burst length
  auto tBL = dram->burstLength / 2 * timing->tCK;

  readToPre = tBL / 2 + timing->tRTP;
  readAP = tBL / 2 + timing->tRTP + timing->tRP;
  readToRead = 2 * timing->tCCD;
  readToWrite = timing->tRL + timing->tDQSCK + tBL / 2 - timing->tWL;
  readToComplete = timing->tRL + timing->tDQSCK + tBL / 2;
  writeToPre = timing->tWL + tBL / 2 + timing->tCK + timing->tWR;
  writeAP = timing->tWL + tBL / 2 + timing->tCK + timing->tWR + timing->tRP;
  writeToRead = timing->tWL + tBL / 2 + timing->tCK + timing->tWTR;
  writeToWrite = 2 * timing->tCCD;
  actToActSame = timing->tRAS + timing->tRP;

  // Create event
  eventCompletion = createEvent([this](uint64_t t, uint64_t) { completion(t); },
                                "Memory::DRAM::Simple::Rank::completion");
}

Rank::~Rank() {}

void Rank::completion(uint64_t now) {
  auto pkt = completionQueue.front();

  completionQueue.pop_front();  // pkt is pointer

  // TODO: Call callback of memory controller with pkt->id

  // Reschedule
  updateCompletion();
}

void Rank::updateCompletion() {
  if (!isScheduled(eventCompletion) && completionQueue.size() > 0) {
    scheduleAbs(eventCompletion, 0, completionQueue.front()->finishedAt);
  }
}

void Rank::submit(Packet *pkt) {
  uint64_t now = getTick();
  auto &bank = banks[pkt->bank];

  switch (pkt->opcode) {
    case Command::Read:
    case Command::ReadAP:
      panic_if(bank.state != BankState::Activate || now < bank.nextRead ||
                   pkt->row != bank.activatedRowIndex,
               "Invalid read access to bank %u.", pkt->bank);

      // Update next possible ticks
      if (pkt->opcode == Command::Read) {
        bank.nextPrecharge = MAX(bank.nextPrecharge, now + readToPre);
      }
      else {
        bank.state = BankState::Idle;
        bank.nextActivate = MAX(bank.nextActivate, now + readAP);
      }

      for (auto &iter : banks) {
        iter.nextRead = MAX(iter.nextRead, now + readToRead);
        iter.nextWrite = MAX(iter.nextWrite, now + readToWrite);
      }

      // Schedule completion of this packet
      pkt->finishedAt = now + readToComplete;
      completionQueue.push_back(pkt);

      updateCompletion();

      break;
    case Command::Write:
    case Command::WriteAP:
      panic_if(bank.state != BankState::Activate || now < bank.nextWrite ||
                   pkt->row != bank.activatedRowIndex,
               "Invalid write access to bank %u.", pkt->bank);

      // Update next possible ticks
      if (pkt->opcode == Command::Write) {
        bank.nextPrecharge = MAX(bank.nextPrecharge, now + writeToPre);
      }
      else {
        bank.state = BankState::Idle;
        bank.nextActivate = MAX(bank.nextActivate, now + writeAP);
      }

      for (auto &iter : banks) {
        iter.nextRead = MAX(iter.nextRead, now + writeToRead);
        iter.nextWrite = MAX(iter.nextWrite, now + writeToWrite);
      }

      // Write is completed immediately in viewpoint of memory controller

      break;
    case Command::Activate:
      panic_if(bank.state != BankState::Idle || now < bank.nextActivate,
               "Invalid activate access to bank %u.", pkt->bank);

      // Update next possible ticks
      for (auto &iter : banks) {
        bank.nextActivate = max(bank.nextActivate, now + timing->tRRD);
      }

      bank.state = BankState::Activate;
      bank.nextActivate = now + actToActSame;
      bank.activatedRowIndex = pkt->row;
      bank.nextWrite = now + timing->tRCD;
      bank.nextRead = now + timing->tRCD;
      bank.nextPrecharge = now + timing->tRAS;

      break;
    case Command::Precharge:
      panic_if(bank.state != BankState::Activate || now < bank.nextPrecharge,
               "Invalid precharge access to bank %u.", pkt->bank);

      // Update next possible ticks
      bank.state = BankState::Idle;
      bank.nextActivate = MAX(bank.nextActivate, now + timing->tRP);

      break;
    case Command::Refresh:
      pendingRefresh = false;

      for (auto &iter : banks) {
        panic_if(bank.state != BankState::Idle, "Invalid refresh access.");
        bank.nextActivate = now + timing->tRFC;
      }

      break;
    default:
      panic("Unknown DRAM command %u.", pkt->opcode);

      break;
  }
}

void Rank::getStatList(std::vector<Stat> &, std::string) noexcept {}

void Rank::getStatValues(std::vector<double> &) noexcept {}

void Rank::resetStatValues() noexcept {}

void Rank::createCheckpoint(std::ostream &) const noexcept {}

void Rank::restoreCheckpoint(std::istream &) noexcept {}

}  // namespace SimpleSSD::Memory::DRAM::Simple
