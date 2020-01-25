// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "mem/dram/simple/bank.hh"

#include "mem/dram/simple/rank.hh"

namespace SimpleSSD::Memory::DRAM::Simple {

Bank::Bank(ObjectData &o, uint8_t id, Rank *r, Timing *t)
    : Object(o),
      parent(r),
      timing(t),
      bankID(id),
      currentPacket(nullptr),
      prevPacketAt(0),
      prevPacketWasRead(false),
      state(BankState::Idle),
      activatedRowIndex(0) {
  // Create event
  eventWork = createEvent([this](uint64_t t, uint64_t) { work(t); },
                          "Memory::DRAM::Simple::Bank::eventWork");
  eventReadDone = createEvent([this](uint64_t t, uint64_t) { completion(t); },
                              "Memory::DRAM::Simple::Bank::eventReadDone");
}

Bank::~Bank() {}

void Bank::updateWork() {
  if (!isScheduled(eventWork)) {
    scheduleNow(eventWork);
  }
}

void Bank::work(uint64_t now) {
  bool clear = true;
  uint64_t delay = 0;

  switch (state) {
    case BankState::Idle:
      if (currentPacket) {
        panic_if(currentPacket->opcode != Command::Refresh &&
                     currentPacket->opcode != Command::Activate,
                 "Invalid command %u when bank is idle.",
                 currentPacket->opcode);

        if (currentPacket->opcode == Command::Activate) {
          state = BankState::Activate;

          activatedRowIndex = currentPacket->row;

          scheduleRel(eventWork, 0, timing->tRCD);

          parent->powerEvent(currentPacket->opcode, bankID);
        }
        else {
          state = BankState::Refresh;

          scheduleRel(eventWork, 0, timing->tRFC);

          parent->powerEvent(currentPacket->opcode, bankID);
        }
      }

      break;
    case BankState::Refresh:
      // Refresh completed
      state = BankState::Idle;

      // Call me again!
      scheduleNow(eventWork);

      break;
    case BankState::Activate:
      if (currentPacket) {
        panic_if(currentPacket->opcode == Command::Refresh ||
                     currentPacket->opcode == Command::Activate,
                 "Invalid command %u when bank is active.",
                 currentPacket->opcode);

        switch (currentPacket->opcode) {
          case Command::Read:
          case Command::ReadAP:
            if ((prevPacketWasRead &&
                 (delay = prevPacketAt + timing->readToRead) > now) ||
                (!prevPacketWasRead &&
                 (delay = prevPacketAt + timing->writeToRead) > now)) {
              // We cannot handle this packet now
              clear = false;

              scheduleRel(eventWork, 0, delay);
            }
            else {
              panic_if(
                  currentPacket->row != activatedRowIndex,
                  "Invalid read access to row %u while row %u is activated.",
                  currentPacket->row, activatedRowIndex);

              prevPacketAt = now;
              prevPacketWasRead = true;

              readStat.add();

              parent->powerEvent(currentPacket->opcode, bankID);

              completionQueue.emplace_back(now + timing->readToComplete,
                                           currentPacket->id);

              updateCompletion();

              // State change to idle if auto-precharge
              if (currentPacket->opcode == Command::ReadAP) {
                state = BankState::Idle;

                scheduleRel(eventWork, 0, timing->readAP);
              }
            }

            break;
          case Command::Write:
          case Command::WriteAP:
            if ((prevPacketWasRead &&
                 (delay = prevPacketAt + timing->readToWrite) > now) ||
                (!prevPacketWasRead &&
                 (delay = prevPacketAt + timing->writeToWrite) > now)) {
              // We cannot handle this packet now
              clear = false;

              scheduleRel(eventWork, 0, delay);
            }
            else {
              panic_if(
                  currentPacket->row != activatedRowIndex,
                  "Invalid write access to row %u while row %u is activated.",
                  currentPacket->row, activatedRowIndex);

              prevPacketAt = now;
              prevPacketWasRead = false;

              writeStat.add();

              parent->powerEvent(currentPacket->opcode, bankID);

              // State change to idle if auto-precharge
              if (currentPacket->opcode == Command::WriteAP) {
                state = BankState::Idle;

                scheduleRel(eventWork, 0, timing->writeAP);
              }
            }

            break;
          case Command::Precharge:
            if ((prevPacketWasRead &&
                 (delay = prevPacketAt + timing->readToPre) > now) ||
                (!prevPacketWasRead &&
                 (delay = prevPacketAt + timing->writeToPre) > now)) {
              // We cannot handle this packet now
              clear = false;

              scheduleRel(eventWork, 0, delay);
            }
            else {
              state = BankState::Precharge;

              parent->powerEvent(Command::Precharge, bankID);

              scheduleRel(eventWork, 0, timing->tRP);
            }

            break;
        }
      }

      break;
    case BankState::Precharge:
      // Precharge completed
      state = BankState::Idle;

      // Call me again!
      scheduleNow(eventWork);

      break;
  }

  if (clear) {
    currentPacket = nullptr;
  }
}

void Bank::updateCompletion() {
  if (!isScheduled(eventReadDone) && !completionQueue.empty()) {
    scheduleAbs(eventReadDone, 0, completionQueue.front().first);
  }
}

void Bank::completion(uint64_t now) {
  auto resp = completionQueue.front();

  completionQueue.pop_front();

  parent->completion(resp.second);

  updateCompletion();
}

bool Bank::submit(Packet *pkt) {
  if (currentPacket) {
    return false;
  }

  currentPacket = pkt;

  updateWork();

  return true;
}

void Bank::getStatList(std::vector<Stat> &list, std::string prefix) noexcept {
  list.emplace_back(prefix + "read", "Read command count");
  list.emplace_back(prefix + "write", "Write command count");
}

void Bank::getStatValues(std::vector<double> &values) noexcept {
  values.push_back((double)readStat.getCount());
  values.push_back((double)writeStat.getCount());
}

void Bank::resetStatValues() noexcept {
  readStat.clear();
  writeStat.clear();
}

void Bank::createCheckpoint(std::ostream &out) const noexcept {
  BACKUP_SCALAR(out, currentPacket);
  BACKUP_SCALAR(out, prevPacketAt);
  BACKUP_SCALAR(out, prevPacketWasRead);
  BACKUP_SCALAR(out, state);
  BACKUP_SCALAR(out, activatedRowIndex);

  uint64_t size = completionQueue.size();

  BACKUP_SCALAR(out, size);

  for (auto &iter : completionQueue) {
    BACKUP_SCALAR(out, iter.first);
    BACKUP_SCALAR(out, iter.second);
  }

  BACKUP_EVENT(out, eventWork);
  BACKUP_EVENT(out, eventReadDone);

  readStat.createCheckpoint(out);
  writeStat.createCheckpoint(out);
}

void Bank::restoreCheckpoint(std::istream &in) noexcept {
  RESTORE_SCALAR(in, currentPacket);

  if (currentPacket) {
    currentPacket = parent->restorePacket(currentPacket);
  }

  RESTORE_SCALAR(in, prevPacketAt);
  RESTORE_SCALAR(in, prevPacketWasRead);
  RESTORE_SCALAR(in, state);
  RESTORE_SCALAR(in, activatedRowIndex);

  uint64_t size;

  RESTORE_SCALAR(in, size);

  for (uint64_t i = 0; i < size; i++) {
    uint64_t f, s;

    RESTORE_SCALAR(in, f);
    RESTORE_SCALAR(in, s);

    completionQueue.emplace_back(f, s);
  }

  RESTORE_EVENT(in, eventWork);
  RESTORE_EVENT(in, eventReadDone);

  readStat.restoreCheckpoint(in);
  writeStat.restoreCheckpoint(in);
}

}  // namespace SimpleSSD::Memory::DRAM::Simple
