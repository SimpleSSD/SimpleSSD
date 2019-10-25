// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 *
 * This file ports dram_ctrl.hh of gem5 simulator to SimpleSSD Memory Subsystem.
 * Copyright of original dram_ctrl.hh is following:
 */
/*
 * Copyright (c) 2012-2019 ARM Limited
 * All rights reserved
 *
 * The license below extends only to copyright in the software and shall
 * not be construed as granting a license to any other intellectual
 * property including but not limited to intellectual property relating
 * to a hardware implementation of the functionality of the software
 * licensed hereunder.  You may use the software subject to the license
 * terms below provided that you ensure that this notice is replicated
 * unmodified and in its entirety in all distributions of the software,
 * modified or unmodified, in source code or in binary form.
 *
 * Copyright (c) 2013 Amin Farmahini-Farahani
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Authors: Andreas Hansson
 *          Ani Udipi
 *          Neha Agarwal
 *          Omar Naji
 *          Wendy Elsasser
 *          Radhika Jagtap
 */

#include "mem/dram/gem5.hh"

#include "util/algorithm.hh"

namespace SimpleSSD::Memory::DRAM {

inline uint64_t mask(int nbits) {
  return (nbits == 64) ? (uint64_t)-1LL : (1ULL << nbits) - 1;
}

template <class T, class B>
inline T insertBits(T val, int first, int last, B bit_val) {
  T t_bit_val = bit_val;
  T bmask = (T)(mask(first - last + 1) << last);
  return (T)(((t_bit_val << last) & bmask) | (val & ~bmask));
}

template <class T>
inline T bits(T val, int first, int last) {
  int nbits = first - last + 1;
  return (T)((val >> last) & mask(nbits));
}

template <class T, class B>
inline void replaceBits(T &val, int first, int last, B bit_val) {
  val = insertBits(val, first, last, bit_val);
}

Data::MemArchitectureSpec DRAMPower::getArchParams(ConfigReader *config) {
  Data::MemArchitectureSpec archSpec;
  auto dram = config->getDRAM();
  auto gem5 = config->getTimingDRAM();

  archSpec.burstLength = dram->burst;
  archSpec.nbrOfBanks = dram->bank;
  archSpec.nbrOfRanks = 1;
  archSpec.dataRate = getDataRate(config);
  archSpec.nbrOfColumns = 0;
  archSpec.nbrOfRows = 0;
  archSpec.width = dram->width;
  archSpec.nbrOfBankGroups = gem5->bankGroup;
  archSpec.dll = gem5->useDLL;
  archSpec.twoVoltageDomains = hasTwoVDD(config);
  archSpec.termination = false;

  return archSpec;
}

Data::MemTimingSpec DRAMPower::getTimingParams(ConfigReader *config) {
  Data::MemTimingSpec timingSpec;
  auto timing = config->getDRAMTiming();

  timingSpec.RC = DIVCEIL((timing->tRAS + timing->tRP), timing->tCK);
  timingSpec.RCD = DIVCEIL(timing->tRCD, timing->tCK);
  timingSpec.RL = DIVCEIL(timing->tCL, timing->tCK);
  timingSpec.RP = DIVCEIL(timing->tRP, timing->tCK);
  timingSpec.RFC = DIVCEIL(timing->tRFC, timing->tCK);
  timingSpec.RAS = DIVCEIL(timing->tRAS, timing->tCK);
  timingSpec.WL = timingSpec.RL - 1;
  timingSpec.DQSCK = 0;
  timingSpec.RTP = DIVCEIL(timing->tRTP, timing->tCK);
  timingSpec.WR = DIVCEIL(timing->tWR, timing->tCK);
  timingSpec.XP = DIVCEIL(timing->tXP, timing->tCK);
  timingSpec.XPDLL = DIVCEIL(timing->tXPDLL, timing->tCK);
  timingSpec.XS = DIVCEIL(timing->tXS, timing->tCK);
  timingSpec.XSDLL = DIVCEIL(timing->tXSDLL, timing->tCK);
  timingSpec.clkPeriod = (timing->tCK / 1000.);

  assert(timingSpec.clkPeriod != 0);
  // panic_if(timingSpec.clkPeriod == 0, "Invalid DRAM clock period");

  timingSpec.clkMhz = (1 / timingSpec.clkPeriod) * 1000.;

  return timingSpec;
}

Data::MemPowerSpec DRAMPower::getPowerParams(ConfigReader *config) {
  Data::MemPowerSpec powerSpec;
  auto power = config->getDRAMPower();

  powerSpec.idd0 = power->pIDD0[0];
  powerSpec.idd02 = power->pIDD0[1];
  powerSpec.idd2p0 = power->pIDD2P0[0];
  powerSpec.idd2p02 = power->pIDD2P0[1];
  powerSpec.idd2p1 = power->pIDD2P1[0];
  powerSpec.idd2p12 = power->pIDD2P1[1];
  powerSpec.idd2n = power->pIDD2N[0];
  powerSpec.idd2n2 = power->pIDD2N[1];
  powerSpec.idd3p0 = power->pIDD3P0[0];
  powerSpec.idd3p02 = power->pIDD3P0[1];
  powerSpec.idd3p1 = power->pIDD3P1[0];
  powerSpec.idd3p12 = power->pIDD3P1[1];
  powerSpec.idd3n = power->pIDD3N[0];
  powerSpec.idd3n2 = power->pIDD3N[1];
  powerSpec.idd4r = power->pIDD4R[0];
  powerSpec.idd4r2 = power->pIDD4R[1];
  powerSpec.idd4w = power->pIDD4W[0];
  powerSpec.idd4w2 = power->pIDD4W[1];
  powerSpec.idd5 = power->pIDD5[0];
  powerSpec.idd52 = power->pIDD5[1];
  powerSpec.idd6 = power->pIDD6[0];
  powerSpec.idd62 = power->pIDD6[1];
  powerSpec.vdd = power->pVDD[0];
  powerSpec.vdd2 = power->pVDD[1];

  return powerSpec;
}

uint8_t DRAMPower::getDataRate(ConfigReader *config) {
  auto timing = config->getDRAMTiming();
  auto dram = config->getDRAM();

  uint32_t burstCycle = DIVCEIL(timing->tBURST, timing->tCK);
  uint8_t dataRate = dram->burst / burstCycle;

  assert(dataRate == 1 || dataRate == 2 || dataRate == 4);
  // panic_if(dataRate != 1 && dataRate != 2 && dataRate != 4,
  //          "Invalid DRAM data rate: %d", dataRate);

  return dataRate;
}

bool DRAMPower::hasTwoVDD(ConfigReader *config) {
  return config->getDRAMPower()->pVDD[1] == 0 ? false : true;
}

Data::MemorySpecification DRAMPower::getMemSpec(ConfigReader *config) {
  Data::MemorySpecification memSpec;

  // TODO: Copying objects!
  memSpec.memArchSpec = getArchParams(config);
  memSpec.memTimingSpec = getTimingParams(config);
  memSpec.memPowerSpec = getPowerParams(config);

  return memSpec;
}

DRAMPower::DRAMPower(ConfigReader *config, bool include_io)
    : powerlib(getMemSpec(config), include_io) {}

void Bank::createCheckpoint(std::ostream &out) const noexcept {
  BACKUP_SCALAR(out, openRow);
  BACKUP_SCALAR(out, bank);
  BACKUP_SCALAR(out, bankgr);
  BACKUP_SCALAR(out, rdAllowedAt);
  BACKUP_SCALAR(out, wrAllowedAt);
  BACKUP_SCALAR(out, preAllowedAt);
  BACKUP_SCALAR(out, actAllowedAt);
  BACKUP_SCALAR(out, rowAccesses);
  BACKUP_SCALAR(out, bytesAccessed);
}

void Bank::restoreCheckpoint(std::istream &in) noexcept {
  RESTORE_SCALAR(in, openRow);
  RESTORE_SCALAR(in, bank);
  RESTORE_SCALAR(in, bankgr);
  RESTORE_SCALAR(in, rdAllowedAt);
  RESTORE_SCALAR(in, wrAllowedAt);
  RESTORE_SCALAR(in, preAllowedAt);
  RESTORE_SCALAR(in, actAllowedAt);
  RESTORE_SCALAR(in, rowAccesses);
  RESTORE_SCALAR(in, bytesAccessed);
}

Rank::Rank(ObjectData &o, TimingDRAM *p, uint8_t r)
    : Object(o),
      parent(p),
      pwrStateTrans(PowerState::Idle),
      pwrStatePostRefresh(PowerState::Idle),
      pwrStateTick(0),
      refreshDueAt(0),
      pwrState(PowerState::Idle),
      refreshState(RefreshState::Idle),
      inLowPowerState(false),
      rank(r),
      readEntries(0),
      writeEntries(0),
      outstandingEvents(0),
      wakeUpAllowedAt(0),
      power(o.config, false),
      banks(o.config->getDRAM()->bank, Bank(o)),
      numBanksActive(0),
      actTicks(o.config->getDRAM()->activationLimit, 0),
      stats(this) {
  writeDoneEvent =
      createEvent([this](uint64_t, uint64_t) { processWriteDoneEvent(); },
                  "Memory::DRAM::Rank::writeDoneEvent");
  activateEvent =
      createEvent([this](uint64_t, uint64_t) { processActivateEvent(); },
                  "Memory::DRAM::Rank::activateEvent");
  prechargeEvent =
      createEvent([this](uint64_t, uint64_t) { processPrechargeEvent(); },
                  "Memory::DRAM::Rank::prechargeEvent");
  refreshEvent =
      createEvent([this](uint64_t, uint64_t) { processRefreshEvent(); },
                  "Memory::DRAM::Rank::refreshEvent");
  powerEvent = createEvent([this](uint64_t, uint64_t) { processPowerEvent(); },
                           "Memory::DRAM::Rank::powerEvent");
  wakeUpEvent =
      createEvent([this](uint64_t, uint64_t) { processWakeUpEvent(); },
                  "Memory::DRAM::Rank::wakeUpEvent");

  timing = object.config->getDRAMTiming();
  gem5Config = object.config->getTimingDRAM();

  auto dram = object.config->getDRAM();

  for (int b = 0; b < dram->bank; b++) {
    banks[b].bank = b;

    if (gem5Config->bankGroup > 0) {
      banks[b].bankgr = b % gem5Config->bankGroup;
    }
    else {
      banks[b].bankgr = b;
    }
  }
}

void Rank::startup(uint64_t ref_tick) {
  panic_if(ref_tick <= pwrStateTick, "Invalid referenct tick.");

  pwrStateTick = getTick();

  scheduleAbs(refreshEvent, 0ull, ref_tick);
}

void Rank::suspend() {
  deschedule(refreshEvent);

  updatePowerStats();

  pwrStatePostRefresh = PowerState::Idle;
}

bool Rank::forceSelfRefreshExit() const {
  return (readEntries != 0) ||
         ((parent->busStateNext == BusState::Write) && (writeEntries != 0));
}

bool Rank::isQueueEmpty() const {
  bool no_queued_cmds =
      ((parent->busStateNext == BusState::Read) && (readEntries == 0)) ||
      ((parent->busStateNext == BusState::Write) && (writeEntries == 0));

  return no_queued_cmds;
}

void Rank::checkDrainDone() {
  if (refreshState == RefreshState::Drain) {
    refreshState = RefreshState::ExitPowerdown;

    scheduleNow(refreshEvent);
  }
}

void Rank::flushCmdList() {
  uint64_t now = getTick();

  sort(cmdList.begin(), cmdList.end(), TimingDRAM::sortTime);

  auto next_iter = cmdList.begin();

  for (; next_iter != cmdList.end(); ++next_iter) {
    Command cmd = *next_iter;
    if (cmd.timestamp <= now) {
      power.powerlib.doCommand(
          cmd.type, cmd.bank,
          DIVCEIL(cmd.timestamp, timing->tCK) - parent->timestampOffset);
    }
    else {
      break;
    }
  }

  cmdList.assign(next_iter, cmdList.end());
}

void Rank::processActivateEvent() {
  if (pwrState != PowerState::Active)
    schedulePowerEvent(PowerState::Active, getTick());
}

void Rank::processPrechargeEvent() {
  assert(outstandingEvents > 0);

  --outstandingEvents;

  if (numBanksActive == 0) {
    uint64_t now = getTick();

    if (isQueueEmpty() && outstandingEvents == 0 &&
        gem5Config->enablePowerdown) {
      assert(pwrState == PowerState::Active);

      powerDownSleep(PowerState::PrechargePowerdown, now);
    }
    else {
      // we should transition to the idle state when the last bank
      // is precharged
      schedulePowerEvent(PowerState::Idle, now);
    }
  }
}

void Rank::processWriteDoneEvent() {
  assert(outstandingEvents > 0);

  --outstandingEvents;
}

void Rank::processRefreshEvent() {
  if ((refreshState == RefreshState::Idle) ||
      (refreshState == RefreshState::ExitSelfRefresh)) {
    refreshDueAt = getTick();
    refreshState = RefreshState::Drain;

    ++outstandingEvents;
  }

  if (refreshState == RefreshState::Drain) {
    if ((rank == parent->activeRank) && (isScheduled(parent->nextReqEvent))) {
      return;
    }
    else {
      refreshState = RefreshState::ExitPowerdown;
    }
  }

  if (refreshState == RefreshState::ExitPowerdown) {
    if (inLowPowerState) {
      scheduleWakeUpEvent(timing->tXP);

      return;
    }
    else {
      refreshState = RefreshState::Precharge;
    }
  }

  if (refreshState == RefreshState::Precharge) {
    if (numBanksActive != 0) {
      uint64_t pre_at = getTick();

      for (auto &b : banks) {
        pre_at = MAX(b.preAllowedAt, pre_at);
      }

      uint64_t act_allowed_at = pre_at + timing->tRP;

      for (auto &b : banks) {
        if (b.openRow != Bank::NO_ROW) {
          parent->prechargeBank(*this, b, pre_at, false);
        }
        else {
          b.actAllowedAt = MAX(b.actAllowedAt, act_allowed_at);
          b.preAllowedAt = MAX(b.preAllowedAt, pre_at);
        }
      }

      cmdList.emplace_back(Command(Data::MemCommand::PREA, 0, pre_at));
    }
    else if ((pwrState == PowerState::Idle) && (outstandingEvents == 1)) {
      schedulePowerEvent(PowerState::Refresh, getTick());
    }
    else {
      assert(isScheduled(prechargeEvent));
    }

    assert(numBanksActive == 0);

    return;
  }

  if (refreshState == RefreshState::Start) {
    assert(numBanksActive == 0);
    assert(pwrState == PowerState::Refresh);

    uint64_t now = getTick();
    uint64_t ref_done_at = now + timing->tRFC;

    for (auto &b : banks) {
      b.actAllowedAt = ref_done_at;
    }

    cmdList.emplace_back(Command(Data::MemCommand::REF, 0, now));

    updatePowerStats();

    refreshDueAt += timing->tREFI;

    warn_if(refreshDueAt < ref_done_at,
            "Refresh was delayed so long we cannot catch up");

    refreshState = RefreshState::Run;

    scheduleAbs(refreshEvent, 0ull, ref_done_at);

    return;
  }

  if (refreshState == RefreshState::Run) {
    // should never get here with any banks active
    assert(numBanksActive == 0);
    assert(pwrState == PowerState::Refresh);

    assert(!isScheduled(powerEvent));

    uint64_t now = getTick();

    if (pwrStatePostRefresh != PowerState::Idle) {
      assert(pwrState == PowerState::Refresh);

      powerDownSleep(pwrState, now);
    }
    else if (isQueueEmpty() && gem5Config->enablePowerdown) {
      assert(outstandingEvents == 1);

      powerDownSleep(PowerState::PrechargePowerdown, now);
    }
    else {
      schedulePowerEvent(PowerState::Idle, now);
    }

    scheduleAbs(refreshEvent, 0ull, refreshDueAt - timing->tRP);
  }
}

void Rank::schedulePowerEvent(PowerState pwr_state, uint64_t tick) {
  if (!isScheduled(powerEvent)) {
    pwrStateTrans = pwr_state;

    scheduleNow(powerEvent);
  }
  else {
    panic(
        "Scheduled power event at %llu to state %d, with scheduled event to %d",
        tick, pwr_state, pwrStateTrans);
  }
}

void Rank::powerDownSleep(PowerState pwr_state, uint64_t tick) {
  if (pwr_state == PowerState::ActivePowerdown) {
    schedulePowerEvent(pwr_state, tick);

    cmdList.emplace_back(Command(Data::MemCommand::PDN_F_ACT, 0, tick));
  }
  else if (pwr_state == PowerState::PrechargePowerdown) {
    schedulePowerEvent(pwr_state, tick);

    cmdList.emplace_back(Command(Data::MemCommand::PDN_F_PRE, 0, tick));
  }
  else if (pwr_state == PowerState::Refresh) {
    // if a refresh just occurred
    // transition to PRE_PDN now that all banks are closed
    // precharge power down requires tCKE to enter. For simplicity
    // this is not considered.
    schedulePowerEvent(PowerState::PrechargePowerdown, tick);
    // push Command to DRAMPower
    cmdList.emplace_back(Command(Data::MemCommand::PDN_F_PRE, 0, tick));
  }
  else if (pwr_state == PowerState::SelfRefresh) {
    assert(pwrStatePostRefresh == PowerState::PrechargePowerdown);

    schedulePowerEvent(PowerState::SelfRefresh, tick);

    cmdList.emplace_back(Command(Data::MemCommand::SREN, 0, tick));
  }

  wakeUpAllowedAt = tick + timing->tCK;

  inLowPowerState = true;
}

void Rank::scheduleWakeUpEvent(uint64_t exit_delay) {
  uint64_t wake_up_tick = MAX(getTick(), wakeUpAllowedAt);

  if (refreshState == RefreshState::ExitPowerdown) {
    pwrStatePostRefresh = pwrState;
  }
  else {
    pwrStatePostRefresh = PowerState::Idle;
  }

  scheduleAbs(wakeUpEvent, 0ull, wake_up_tick);

  for (auto &b : banks) {
    b.wrAllowedAt = MAX(wake_up_tick + exit_delay, b.wrAllowedAt);
    b.rdAllowedAt = MAX(wake_up_tick + exit_delay, b.rdAllowedAt);
    b.preAllowedAt = MAX(wake_up_tick + exit_delay, b.preAllowedAt);
    b.actAllowedAt = MAX(wake_up_tick + exit_delay, b.actAllowedAt);
  }

  inLowPowerState = false;

  if (pwrStateTrans == PowerState::ActivePowerdown) {
    cmdList.emplace_back(Command(Data::MemCommand::PUP_ACT, 0, wake_up_tick));
  }
  else if (pwrStateTrans == PowerState::PrechargePowerdown) {
    cmdList.emplace_back(Command(Data::MemCommand::PUP_PRE, 0, wake_up_tick));
  }
  else if (pwrStateTrans == PowerState::SelfRefresh) {
    cmdList.emplace_back(Command(Data::MemCommand::SREX, 0, wake_up_tick));
  }
}

void Rank::processWakeUpEvent() {
  // Should be in a power-down or self-refresh state
  assert((pwrState == PowerState::ActivePowerdown) ||
         (pwrState == PowerState::PrechargePowerdown) ||
         (pwrState == PowerState::SelfRefresh));

  // Check current state to determine transition state
  if (pwrState == PowerState::ActivePowerdown) {
    // banks still open, transition to PowerState::Active
    schedulePowerEvent(PowerState::Active, getTick());
  }
  else {
    // transitioning from a precharge power-down or self-refresh state
    // banks are closed - transition to PowerState::Idle
    schedulePowerEvent(PowerState::Idle, getTick());
  }
}

void Rank::processPowerEvent() {
  uint64_t now = getTick();

  assert(now >= pwrStateTick);

  uint64_t duration = now - pwrStateTick;
  PowerState prev_state = pwrState;

  if ((prev_state == PowerState::PrechargePowerdown) ||
      (prev_state == PowerState::ActivePowerdown) ||
      (prev_state == PowerState::SelfRefresh)) {
    stats.totalIdleTime += duration;
  }

  pwrState = pwrStateTrans;
  pwrStateTick = now;

  if (prev_state == PowerState::Refresh) {
    assert(outstandingEvents == 1);

    --outstandingEvents;
    refreshState = RefreshState::Idle;

    if (pwrState != PowerState::Idle) {
      assert(pwrState == PowerState::PrechargePowerdown);
    }

    if (!isScheduled(parent->nextReqEvent)) {
      scheduleNow(parent->nextReqEvent);
    }
  }

  if ((pwrState == PowerState::Active) &&
      (refreshState == RefreshState::ExitPowerdown)) {
    assert(prev_state == PowerState::ActivePowerdown);

    refreshState = RefreshState::Precharge;
    scheduleNow(refreshEvent);
  }
  else if (pwrState == PowerState::Idle) {
    if (prev_state == PowerState::SelfRefresh) {
      refreshState = RefreshState::ExitSelfRefresh;
      scheduleRel(refreshEvent, 0ull, timing->tXS);
    }
    else {
      if ((refreshState == RefreshState::Precharge) ||
          (refreshState == RefreshState::ExitPowerdown)) {
        if (!isScheduled(activateEvent)) {
          assert(!isScheduled(powerEvent));

          if (refreshState == RefreshState::ExitPowerdown) {
            assert(prev_state == PowerState::PrechargePowerdown);

            schedulePowerEvent(PowerState::Refresh, now + timing->tXP);
          }
          else if (refreshState == RefreshState::Precharge) {
            pwrState = PowerState::Refresh;
          }
        }
        else {
          assert(isScheduled(prechargeEvent));
        }
      }
    }
  }

  if (pwrState == PowerState::Refresh) {
    assert(refreshState == RefreshState::Precharge ||
           refreshState == RefreshState::ExitPowerdown);

    if (pwrStatePostRefresh == PowerState::PrechargePowerdown &&
        isQueueEmpty() && gem5Config->enablePowerdown) {
      powerDownSleep(PowerState::SelfRefresh, now);

      assert(outstandingEvents == 1);
      --outstandingEvents;

      pwrState = PowerState::Idle;
    }
    else {
      assert(!isScheduled(powerEvent));

      scheduleNow(refreshEvent);

      refreshState = RefreshState::Start;
    }
  }
}

void Rank::updatePowerStats() {
  uint64_t now = getTick();
  auto chip = object.config->getDRAM()->chip;

  flushCmdList();

  power.powerlib.calcWindowEnergy(DIVCEIL(now, timing->tCK) -
                                  parent->timestampOffset);

  auto energy = power.powerlib.getEnergy();

  stats.actEnergy += energy.act_energy * chip;
  stats.preEnergy += energy.pre_energy * chip;
  stats.readEnergy += energy.read_energy * chip;
  stats.writeEnergy += energy.write_energy * chip;
  stats.refreshEnergy += energy.ref_energy * chip;
  stats.actBackEnergy += energy.act_stdby_energy * chip;
  stats.preBackEnergy += energy.pre_stdby_energy * chip;
  stats.actPowerDownEnergy += energy.f_act_pd_energy * chip;
  stats.prePowerDownEnergy += energy.f_pre_pd_energy * chip;
  stats.selfRefreshEnergy += energy.sref_energy * chip;

  stats.totalEnergy += energy.window_energy * chip;

  // Average power must not be accumulated but calculated over the time
  // since last stats reset. SimClock::Frequency is tick period not tick
  // frequency.
  //              energy (pJ)     1e-9
  // power (mW) = ----------- * ----------
  //              time (tick)   tick_frequency
  stats.averagePower =
      (stats.totalEnergy / (now - parent->lastStatsResetTick)) * 1000.0;
}

void Rank::computeStats() {
  updatePowerStats();

  pwrStateTick = getTick();
}

void Rank::getStatList(std::vector<Stat> &list, std::string prefix) noexcept {
  stats.getStatList(list, prefix);
}

void Rank::getStatValues(std::vector<double> &values) noexcept {
  stats.getStatValues(values);
}

void Rank::resetStatValues() noexcept {
  stats.resetStatValues();
  power.powerlib.calcWindowEnergy(DIVCEIL(getTick(), timing->tCK) -
                                  parent->timestampOffset);
}

void Rank::createCheckpoint(std::ostream &out) const noexcept {
  BACKUP_SCALAR(out, pwrStateTrans);
  BACKUP_SCALAR(out, pwrStatePostRefresh);
  BACKUP_SCALAR(out, pwrStateTick);
  BACKUP_SCALAR(out, refreshDueAt);
  BACKUP_SCALAR(out, pwrState);
  BACKUP_SCALAR(out, refreshState);
  BACKUP_SCALAR(out, inLowPowerState);
  BACKUP_SCALAR(out, rank);
  BACKUP_SCALAR(out, readEntries);
  BACKUP_SCALAR(out, writeEntries);
  BACKUP_SCALAR(out, outstandingEvents);
  BACKUP_SCALAR(out, wakeUpAllowedAt);
  BACKUP_SCALAR(out, numBanksActive);
  BACKUP_EVENT(out, writeDoneEvent);
  BACKUP_EVENT(out, activateEvent);
  BACKUP_EVENT(out, prechargeEvent);
  BACKUP_EVENT(out, refreshEvent);
  BACKUP_EVENT(out, powerEvent);
  BACKUP_EVENT(out, wakeUpEvent);

  uint64_t size = cmdList.size();
  BACKUP_SCALAR(out, size);

  for (auto &iter : cmdList) {
    BACKUP_SCALAR(out, iter.bank);
    BACKUP_SCALAR(out, iter.type);
    BACKUP_SCALAR(out, iter.timestamp);
  }

  size = banks.size();
  BACKUP_SCALAR(out, size);

  for (auto &iter : banks) {
    iter.createCheckpoint(out);
  }

  size = actTicks.size();
  BACKUP_SCALAR(out, size);

  for (auto &iter : actTicks) {
    BACKUP_SCALAR(out, iter);
  }

  stats.createCheckpoint(out);
}

void Rank::restoreCheckpoint(std::istream &in) noexcept {
  RESTORE_SCALAR(in, pwrStateTrans);
  RESTORE_SCALAR(in, pwrStatePostRefresh);
  RESTORE_SCALAR(in, pwrStateTick);
  RESTORE_SCALAR(in, refreshDueAt);
  RESTORE_SCALAR(in, pwrState);
  RESTORE_SCALAR(in, refreshState);
  RESTORE_SCALAR(in, inLowPowerState);
  RESTORE_SCALAR(in, rank);
  RESTORE_SCALAR(in, readEntries);
  RESTORE_SCALAR(in, writeEntries);
  RESTORE_SCALAR(in, outstandingEvents);
  RESTORE_SCALAR(in, wakeUpAllowedAt);
  RESTORE_SCALAR(in, numBanksActive);
  RESTORE_EVENT(in, writeDoneEvent);
  RESTORE_EVENT(in, activateEvent);
  RESTORE_EVENT(in, prechargeEvent);
  RESTORE_EVENT(in, refreshEvent);
  RESTORE_EVENT(in, powerEvent);
  RESTORE_EVENT(in, wakeUpEvent);

  uint64_t size;
  RESTORE_SCALAR(in, size);

  cmdList.reserve(size);

  for (uint64_t i = 0; i < size; i++) {
    Data::MemCommand::cmds cmd;
    uint8_t bank;
    uint64_t time;

    RESTORE_SCALAR(in, bank);
    RESTORE_SCALAR(in, cmd);
    RESTORE_SCALAR(in, time);

    cmdList.emplace_back(Command(cmd, bank, time));
  }

  RESTORE_SCALAR(in, size);

  for (uint64_t i = 0; i < size; i++) {
    banks.at(i).restoreCheckpoint(in);
  }

  RESTORE_SCALAR(in, size);

  for (uint64_t i = 0; i < size; i++) {
    uint32_t tmp;

    RESTORE_SCALAR(in, tmp);
    actTicks.emplace_back(tmp);
  }

  stats.restoreCheckpoint(in);
}

RankStats::RankStats(Rank *p) : parent(p) {}

void RankStats::getStatList(std::vector<Stat> &list,
                            std::string prefix) noexcept {
  Stat temp;

  temp.name = prefix + "actEnergy";
  temp.desc = "Energy for activate commands per rank (pJ)";
  list.push_back(temp);

  temp.name = prefix + "preEnergy";
  temp.desc = "Energy for precharge commands per rank (pJ)";
  list.push_back(temp);

  temp.name = prefix + "readEnergy";
  temp.desc = "Energy for read commands per rank (pJ)";
  list.push_back(temp);

  temp.name = prefix + "writeEnergy";
  temp.desc = "Energy for write commands per rank (pJ)";
  list.push_back(temp);

  temp.name = prefix + "refreshEnergy";
  temp.desc = "Energy for refresh commands per rank (pJ)";
  list.push_back(temp);

  temp.name = prefix + "actBackEnergy";
  temp.desc = "Energy for active background per rank (pJ)";
  list.push_back(temp);

  temp.name = prefix + "preBackEnergy";
  temp.desc = "Energy for precharge background per rank (pJ)";
  list.push_back(temp);

  temp.name = prefix + "actPowerDownEnergy";
  temp.desc = "Energy for active power-down per rank (pJ)";
  list.push_back(temp);

  temp.name = prefix + "prePowerDownEnergy";
  temp.desc = "Energy for precharge power-down per rank (pJ)";
  list.push_back(temp);

  temp.name = prefix + "selfRefreshEnergy";
  temp.desc = "Energy for self refresh per rank (pJ)";
  list.push_back(temp);

  temp.name = prefix + "totalEnergy";
  temp.desc = "Total energy per rank (pJ)";
  list.push_back(temp);

  temp.name = prefix + "averagePower";
  temp.desc = "Core power per rank (mW)";
  list.push_back(temp);

  temp.name = prefix + "totalIdleTime";
  temp.desc = "Total Idle time Per DRAM Rank";
  list.push_back(temp);
}

void RankStats::getStatValues(std::vector<double> &values) noexcept {
  values.push_back(actEnergy);
  values.push_back(preEnergy);
  values.push_back(readEnergy);
  values.push_back(writeEnergy);
  values.push_back(refreshEnergy);
  values.push_back(actBackEnergy);
  values.push_back(preBackEnergy);
  values.push_back(actPowerDownEnergy);
  values.push_back(prePowerDownEnergy);
  values.push_back(selfRefreshEnergy);
  values.push_back(totalEnergy);
  values.push_back(averagePower);
  values.push_back(totalIdleTime);
}

void RankStats::resetStatValues() noexcept {
  actEnergy = 0.;
  preEnergy = 0.;
  readEnergy = 0.;
  writeEnergy = 0.;
  refreshEnergy = 0.;
  actBackEnergy = 0.;
  preBackEnergy = 0.;
  actPowerDownEnergy = 0.;
  prePowerDownEnergy = 0.;
  selfRefreshEnergy = 0.;
  totalEnergy = 0.;
  averagePower = 0.;
  totalIdleTime = 0.;
}

void RankStats::createCheckpoint(std::ostream &out) const noexcept {
  BACKUP_SCALAR(out, actEnergy);
  BACKUP_SCALAR(out, preEnergy);
  BACKUP_SCALAR(out, readEnergy);
  BACKUP_SCALAR(out, writeEnergy);
  BACKUP_SCALAR(out, refreshEnergy);
  BACKUP_SCALAR(out, actBackEnergy);
  BACKUP_SCALAR(out, preBackEnergy);
  BACKUP_SCALAR(out, actPowerDownEnergy);
  BACKUP_SCALAR(out, prePowerDownEnergy);
  BACKUP_SCALAR(out, selfRefreshEnergy);
  BACKUP_SCALAR(out, totalEnergy);
  BACKUP_SCALAR(out, averagePower);
  BACKUP_SCALAR(out, totalIdleTime);
}

void RankStats::restoreCheckpoint(std::istream &in) noexcept {
  RESTORE_SCALAR(in, actEnergy);
  RESTORE_SCALAR(in, preEnergy);
  RESTORE_SCALAR(in, readEnergy);
  RESTORE_SCALAR(in, writeEnergy);
  RESTORE_SCALAR(in, refreshEnergy);
  RESTORE_SCALAR(in, actBackEnergy);
  RESTORE_SCALAR(in, preBackEnergy);
  RESTORE_SCALAR(in, actPowerDownEnergy);
  RESTORE_SCALAR(in, prePowerDownEnergy);
  RESTORE_SCALAR(in, selfRefreshEnergy);
  RESTORE_SCALAR(in, totalEnergy);
  RESTORE_SCALAR(in, averagePower);
  RESTORE_SCALAR(in, totalIdleTime);
}

DRAMStats::DRAMStats(TimingDRAM *p) : parent(p) {}

void DRAMStats::getStatList(std::vector<Stat> &list,
                            std::string prefix) noexcept {
  Stat temp;

  temp.name = prefix + "readReqs";
  temp.desc = "Number of read requests accepted";
  list.push_back(temp);

  temp.name = prefix + "writeReqs";
  temp.desc = "Number of write requests accepted";
  list.push_back(temp);

  temp.name = prefix + "readBursts";
  temp.desc =
      "Number of DRAM read bursts, including those serviced by the write queue";
  list.push_back(temp);

  temp.name = prefix + "writeBursts";
  temp.desc =
      "Number of DRAM write bursts, including those merged in the write queue";
  list.push_back(temp);

  temp.name = prefix + "servicedByWrQ";
  temp.desc = "Number of DRAM read bursts serviced by the write queue";
  list.push_back(temp);

  temp.name = prefix + "mergedWrBursts";
  temp.desc = "Number of DRAM write bursts merged with an existing one";
  list.push_back(temp);

  temp.name = prefix + "neitherReadNorWriteReqs";
  temp.desc = "Number of requests that are neither read nor write";
  list.push_back(temp);

  temp.name = prefix + "totQLat";
  temp.desc = "Total ticks spent queuing";
  list.push_back(temp);

  temp.name = prefix + "totBusLat";
  temp.desc = "Total ticks spent in databus transfers";
  list.push_back(temp);

  temp.name = prefix + "totMemAccLat";
  temp.desc =
      "Total ticks spent from burst creation until serviced by the DRAM";
  list.push_back(temp);

  temp.name = prefix + "numRdRetry";
  temp.desc = "Number of times read queue was full causing retry";
  list.push_back(temp);

  temp.name = prefix + "numWrRetry";
  temp.desc = "Number of times write queue was full causing retry";
  list.push_back(temp);

  temp.name = prefix + "bytesReadDRAM";
  temp.desc = "Total number of bytes read from DRAM";
  list.push_back(temp);

  temp.name = prefix + "bytesReadWrQ";
  temp.desc = "Total number of bytes read from write queue";
  list.push_back(temp);

  temp.name = prefix + "bytesWritten";
  temp.desc = "Total number of bytes written to DRAM";
  list.push_back(temp);

  temp.name = prefix + "avgRdBW";
  temp.desc = "Average DRAM read bandwidth in MiByte/s";
  list.push_back(temp);

  temp.name = prefix + "avgWrBW";
  temp.desc = "Average achieved write bandwidth in MiByte/s";
  list.push_back(temp);

  temp.name = prefix + "peakBW";
  temp.desc = "Theoretical peak bandwidth in MiByte/s";
  list.push_back(temp);

  temp.name = prefix + "busUtil";
  temp.desc = "Data bus utilization in percentage";
  list.push_back(temp);

  temp.name = prefix + "busUtilRead";
  temp.desc = "Data bus utilization in percentage for reads";
  list.push_back(temp);

  temp.name = prefix + "busUtilWrite";
  temp.desc = "Data bus utilization in percentage for writes";
  list.push_back(temp);

  temp.name = prefix + "totGap";
  temp.desc = "Total gap between requests";
  list.push_back(temp);

  temp.name = prefix + "avgGap";
  temp.desc = "Average gap between requests";
  list.push_back(temp);
}

void DRAMStats::getStatValues(std::vector<double> &values,
                              double secs) noexcept {
  avgQLat = totQLat / (readBursts - servicedByWrQ);
  avgBusLat = totBusLat / (readBursts - servicedByWrQ);
  avgMemAccLat = totMemAccLat / (readBursts - servicedByWrQ);

  avgRdBW = (bytesReadDRAM / 1000000.) / secs;
  avgWrBW = (bytesWritten / 1000000.) / secs;
  peakBW = (1000000000000. / parent->pTiming->tBURST) *
           parent->pStructure->burst / 1000000.;

  busUtil = (avgRdBW + avgWrBW) / peakBW * 100;
  busUtilRead = avgRdBW / peakBW * 100;
  busUtilWrite = avgWrBW / peakBW * 100;

  avgGap = totGap / (readReqs + writeReqs);

  values.push_back(readReqs);
  values.push_back(writeReqs);
  values.push_back(readBursts);
  values.push_back(writeBursts);
  values.push_back(servicedByWrQ);
  values.push_back(mergedWrBursts);
  values.push_back(neitherReadNorWriteReqs);
  values.push_back(totQLat);
  values.push_back(totBusLat);
  values.push_back(totMemAccLat);
  values.push_back(avgQLat);
  values.push_back(avgBusLat);
  values.push_back(avgMemAccLat);
  values.push_back(numRdRetry);
  values.push_back(numWrRetry);
  values.push_back(bytesReadDRAM);
  values.push_back(bytesReadWrQ);
  values.push_back(bytesWritten);
  values.push_back(avgRdBW);
  values.push_back(avgWrBW);
  values.push_back(peakBW);
  values.push_back(busUtil);
  values.push_back(busUtilRead);
  values.push_back(busUtilWrite);
  values.push_back(totGap);
  values.push_back(avgGap);
}

void DRAMStats::resetStatValues() noexcept {
  readReqs = 0.;
  writeReqs = 0.;
  readBursts = 0.;
  writeBursts = 0.;
  servicedByWrQ = 0.;
  mergedWrBursts = 0.;
  neitherReadNorWriteReqs = 0.;
  totQLat = 0.;
  totBusLat = 0.;
  totMemAccLat = 0.;
  avgQLat = 0.;
  avgBusLat = 0.;
  avgMemAccLat = 0.;
  numRdRetry = 0.;
  numWrRetry = 0.;
  bytesReadDRAM = 0.;
  bytesReadWrQ = 0.;
  bytesWritten = 0.;
  avgRdBW = 0.;
  avgWrBW = 0.;
  peakBW = 0.;
  busUtil = 0.;
  busUtilRead = 0.;
  busUtilWrite = 0.;
}

void DRAMStats::createCheckpoint(std::ostream &out) const noexcept {
  BACKUP_SCALAR(out, readReqs);
  BACKUP_SCALAR(out, writeReqs);
  BACKUP_SCALAR(out, readBursts);
  BACKUP_SCALAR(out, writeBursts);
  BACKUP_SCALAR(out, servicedByWrQ);
  BACKUP_SCALAR(out, mergedWrBursts);
  BACKUP_SCALAR(out, neitherReadNorWriteReqs);
  BACKUP_SCALAR(out, totQLat);
  BACKUP_SCALAR(out, totBusLat);
  BACKUP_SCALAR(out, totMemAccLat);
  BACKUP_SCALAR(out, avgQLat);
  BACKUP_SCALAR(out, avgBusLat);
  BACKUP_SCALAR(out, avgMemAccLat);
  BACKUP_SCALAR(out, numRdRetry);
  BACKUP_SCALAR(out, numWrRetry);
  BACKUP_SCALAR(out, bytesReadDRAM);
  BACKUP_SCALAR(out, bytesReadWrQ);
  BACKUP_SCALAR(out, bytesWritten);
  BACKUP_SCALAR(out, avgRdBW);
  BACKUP_SCALAR(out, avgWrBW);
  BACKUP_SCALAR(out, peakBW);
  BACKUP_SCALAR(out, busUtil);
  BACKUP_SCALAR(out, busUtilRead);
  BACKUP_SCALAR(out, busUtilWrite);
  BACKUP_SCALAR(out, totGap);
  BACKUP_SCALAR(out, avgGap);
}

void DRAMStats::restoreCheckpoint(std::istream &in) noexcept {
  RESTORE_SCALAR(in, readReqs);
  RESTORE_SCALAR(in, writeReqs);
  RESTORE_SCALAR(in, readBursts);
  RESTORE_SCALAR(in, writeBursts);
  RESTORE_SCALAR(in, servicedByWrQ);
  RESTORE_SCALAR(in, mergedWrBursts);
  RESTORE_SCALAR(in, neitherReadNorWriteReqs);
  RESTORE_SCALAR(in, totQLat);
  RESTORE_SCALAR(in, totBusLat);
  RESTORE_SCALAR(in, totMemAccLat);
  RESTORE_SCALAR(in, avgQLat);
  RESTORE_SCALAR(in, avgBusLat);
  RESTORE_SCALAR(in, avgMemAccLat);
  RESTORE_SCALAR(in, numRdRetry);
  RESTORE_SCALAR(in, numWrRetry);
  RESTORE_SCALAR(in, bytesReadDRAM);
  RESTORE_SCALAR(in, bytesReadWrQ);
  RESTORE_SCALAR(in, bytesWritten);
  RESTORE_SCALAR(in, avgRdBW);
  RESTORE_SCALAR(in, avgWrBW);
  RESTORE_SCALAR(in, peakBW);
  RESTORE_SCALAR(in, busUtil);
  RESTORE_SCALAR(in, busUtilRead);
  RESTORE_SCALAR(in, busUtilWrite);
  RESTORE_SCALAR(in, totGap);
  RESTORE_SCALAR(in, avgGap);
}

TimingDRAM::TimingDRAM(ObjectData &o)
    : AbstractDRAM(o),
      gem5Config(o.config->getTimingDRAM()),
      burstSize((pStructure->chip * pStructure->burst * pStructure->width) / 8),
      rowBufferSize(pStructure->chip * gem5Config->rowBufferSize),
      columnsPerRowBuffer(rowBufferSize / burstSize),
      columnsPerStripe(1),
      bankGroupArch(gem5Config->bankGroup > 0),
      writeHighThreshold((uint32_t)(gem5Config->writeBufferSize *
                                    gem5Config->forceWriteThreshold)),
      writeLowThreshold((uint32_t)(gem5Config->writeBufferSize *
                                   gem5Config->startWriteThreshold)),
      rankToRankDly(pTiming->tCS + pTiming->tBURST),
      wrToRdDly(pTiming->tCL + pTiming->tBURST + pTiming->tWTR),
      rdToWrDly(pTiming->tRTW + pTiming->tBURST),
      rowsPerBank(0),
      writesThisTime(0),
      readsThisTime(0),
      retryRdReq(false),
      retryWrReq(false),
      nextBurstAt(0),
      prevArrival(0),
      nextReqTime(0),
      stats(this),
      activeRank(0),
      timestampOffset(0),
      lastStatsResetTick(0),
      busState(BusState::Read),
      busStateNext(BusState::Read),
      totalReadQueueSize(0),
      totalWriteQueueSize(0) {
  nextReqEvent =
      createEvent([this](uint64_t, uint64_t) { processNextReqEvent(); },
                  "Memory::DRAM::TimingDRAM::nextReqEvent");
  respondEvent =
      createEvent([this](uint64_t, uint64_t) { processRespondEvent(); },
                  "Memory::DRAM::TimingDRAM::respondEvent");

  panic_if(popcount32(pStructure->rank) != 1,
           "DRAM rank count of %d is not allowed, must be a power of two",
           pStructure->rank);

  panic_if(popcount32(pStructure->burst) != 1,
           "DRAM burst size %d is not allowed, must be a power of two",
           pStructure->burst);

  for (int i = 0; i < pStructure->burst; i++) {
    Rank *rank = new Rank(object, this, i);
    ranks.emplace_back(rank);
  }

  if (gem5Config->startWriteThreshold >= gem5Config->forceWriteThreshold)
    panic("Write buffer low threshold %d must be smaller than the  high "
          "threshold %d",
          gem5Config->startWriteThreshold, gem5Config->forceWriteThreshold);

  capacity =
      pStructure->chipSize / (1048576) * pStructure->chip * pStructure->burst;
  rowsPerBank = (uint32_t)(
      capacity / (rowBufferSize * pStructure->bank * pStructure->burst));

  if (pTiming->tREFI <= pTiming->tRP || pTiming->tREFI <= pTiming->tRFC) {
    panic("tREFI (%u) must be larger than tRP (%u) and tRFC (%u)",
          pTiming->tREFI, pTiming->tRP, pTiming->tRFC);
  }

  if (bankGroupArch) {
    if (gem5Config->bankGroup > pStructure->bank) {
      panic("banks per rank (%u) must be equal to or larger than banks groups "
            "per rank (%u)",
            pStructure->bank, gem5Config->bankGroup);
    }
    if ((pStructure->bank % gem5Config->bankGroup) != 0) {
      panic("Banks per rank (%u) must be evenly divisible by bank groups per "
            "rank (%u) for equal banks per bank group",
            pStructure->bank, gem5Config->bankGroup);
    }
    if (pTiming->tCCD_L <= pTiming->tBURST) {
      panic("tCCD_L (%u) should be larger than tBURST (%u) when bank groups "
            "per rank (%u) is greater than 1",
            pTiming->tCCD_L, pTiming->tBURST, gem5Config->bankGroup);
    }
    if (pTiming->tCCD_L_WR <= pTiming->tBURST) {
      panic("tCCD_L_WR (%u) should be larger than tBURST (%u) when bank groups "
            "per rank (%u) is greater than 1",
            pTiming->tCCD_L_WR, pTiming->tBURST, gem5Config->bankGroup);
    }
    if (pTiming->tRRD_L < pTiming->tRRD) {
      panic("tRRD_L (%u) should be larger than tRRD (%u) when bank groups per "
            "rank (%u) is greater than 1",
            pTiming->tRRD_L, pTiming->tRRD, gem5Config->bankGroup);
    }
  }

  timestampOffset = DIVCEIL(0, pTiming->tCK);

  for (auto r : ranks) {
    r->startup(pTiming->tREFI - pTiming->tRP);
  }

  nextBurstAt = pTiming->tRP + pTiming->tRCD;
}

bool TimingDRAM::readQueueFull(uint32_t neededEntries) const {
  auto rdsize_new = totalReadQueueSize + respQueue.size() + neededEntries;
  return rdsize_new > gem5Config->readBufferSize;
}

bool TimingDRAM::writeQueueFull(uint32_t neededEntries) const {
  auto wrsize_new = (totalWriteQueueSize + neededEntries);
  return wrsize_new > gem5Config->writeBufferSize;
}

DRAMPacket *TimingDRAM::decodeAddr(uint64_t dramPktAddr, unsigned size,
                                   bool isRead) {
  uint16_t rank = 0;
  uint8_t bank = 0;
  uint32_t row = 0;

  uint64_t addr = dramPktAddr / burstSize;

  if (gem5Config->mapping == Config::AddressMapping::RoRaBaChCo) {
    addr = addr / columnsPerRowBuffer;
    addr = addr / pStructure->channel;
    bank = addr % pStructure->bank;
    addr = addr / pStructure->bank;
    rank = addr % pStructure->burst;
    addr = addr / pStructure->burst;
    row = addr % rowsPerBank;
  }
  else if (gem5Config->mapping == Config::AddressMapping::RoRaBaCoCh) {
    addr = addr / columnsPerStripe;
    addr = addr / pStructure->channel;
    addr = addr / (columnsPerRowBuffer / columnsPerStripe);
    bank = addr % pStructure->bank;
    addr = addr / pStructure->bank;
    rank = addr % pStructure->burst;
    addr = addr / pStructure->burst;
    row = addr % rowsPerBank;
  }
  else if (gem5Config->mapping == Config::AddressMapping::RoCoRaBaCh) {
    addr = addr / columnsPerStripe;
    addr = addr / pStructure->channel;
    bank = addr % pStructure->bank;
    addr = addr / pStructure->bank;
    rank = addr % pStructure->burst;
    addr = addr / pStructure->burst;
    addr = addr / (columnsPerRowBuffer / columnsPerStripe);
    row = addr % rowsPerBank;
  }
  else {
    panic("Unknown address mapping policy chosen!");
  }

  assert(rank < pStructure->burst);
  assert(bank < pStructure->bank);
  assert(row < rowsPerBank);
  assert(row < Bank::NO_ROW);

  // create the corresponding DRAM packet with the entry time and
  // ready time set to the current tick, the latter will be updated
  // later
  uint16_t bank_id = pStructure->bank * rank + bank;

  return new DRAMPacket(getTick(), isRead, (uint8_t)rank, bank, row, bank_id,
                        dramPktAddr, size, ranks[rank]->banks[bank],
                        *ranks[rank]);
}

bool TimingDRAM::addToReadQueue(uint64_t addr, uint32_t pktsize,
                                uint32_t pktCount, Event eid, uint64_t data) {
  unsigned pktsServicedByWrQ = 0;

  BurstHelper *burst_helper = NULL;
  for (uint32_t cnt = 0; cnt < pktCount; ++cnt) {
    auto size = MIN((addr | (burstSize - 1)) + 1, addr + pktsize) - addr;

    stats.readBursts++;

    bool foundInWrQ = false;
    uint64_t burst_addr = burstAlign(addr);

    if (isInWriteQueue.find(burst_addr) != isInWriteQueue.end()) {
      for (const auto &p : writeQueue) {
        if (p->addr <= addr && ((addr + size) <= (p->addr + p->size))) {
          foundInWrQ = true;

          pktsServicedByWrQ++;

          stats.servicedByWrQ++;
          stats.bytesReadWrQ += burstSize;

          break;
        }
      }
    }

    if (!foundInWrQ) {
      if (pktCount > 1 && burst_helper == NULL) {
        burst_helper = new BurstHelper(pktCount);
      }

      auto *dram_pkt = decodeAddr(addr, (uint32_t)size, true);
      dram_pkt->burstHelper = burst_helper;
      dram_pkt->eid = eid;
      dram_pkt->data = data;

      assert(!readQueueFull(1));

      readQueue.emplace_back(dram_pkt);

      ++dram_pkt->rankRef.readEntries;

      logRequest(BusState::Read, 1);
    }

    addr = (addr | (burstSize - 1)) + 1;
  }

  if (pktsServicedByWrQ == 1) {
    return false;
  }

  if (burst_helper != NULL)
    burst_helper->burstsServiced = pktsServicedByWrQ;

  if (!isScheduled(nextReqEvent)) {
    scheduleNow(nextReqEvent);
  }

  return true;
}

bool TimingDRAM::addToWriteQueue(uint64_t addr, uint32_t pktsize,
                                 uint32_t pktCount) {
  for (uint32_t cnt = 0; cnt < pktCount; ++cnt) {
    auto size = MIN((addr | (burstSize - 1)) + 1, addr + pktsize) - addr;

    stats.writeBursts++;

    bool merged = isInWriteQueue.find(burstAlign(addr)) != isInWriteQueue.end();

    if (!merged) {
      DRAMPacket *dram_pkt = decodeAddr(addr, (uint32_t)size, false);

      assert(totalWriteQueueSize < gem5Config->writeBufferSize);

      writeQueue.emplace_back(dram_pkt);
      isInWriteQueue.insert(burstAlign(addr));

      logRequest(BusState::Write, 1);

      assert(totalWriteQueueSize == isInWriteQueue.size());

      ++dram_pkt->rankRef.writeEntries;
    }
    else {
      stats.mergedWrBursts++;
    }

    addr = (addr | (burstSize - 1)) + 1;
  }

  if (!isScheduled(nextReqEvent)) {
    scheduleNow(nextReqEvent);
  }

  return false;
}

bool TimingDRAM::receive(uint64_t address, uint32_t size, bool read, Event eid,
                         uint64_t data) {
  bool ret = false;
  uint64_t now = getTick();

  if (prevArrival != 0) {
    stats.totGap += now - prevArrival;
  }

  prevArrival = now;

  auto offset = address & (burstSize - 1);
  uint32_t dram_pkt_count = (uint32_t)DIVCEIL(offset + size, burstSize);

  if (!read) {
    assert(size != 0);

    if (writeQueueFull(dram_pkt_count)) {
      retryWrReq = true;
      stats.numWrRetry++;
      return false;
    }
    else {
      ret = addToWriteQueue(address, size, dram_pkt_count);
      stats.writeReqs++;
    }
  }
  else {
    assert(size != 0);

    if (readQueueFull(dram_pkt_count)) {
      retryRdReq = true;
      stats.numRdRetry++;
      return false;
    }
    else {
      ret = addToReadQueue(address, size, dram_pkt_count, eid, data);
      stats.readReqs++;
    }
  }

  return ret;
}

void TimingDRAM::processRespondEvent() {
  auto *dram_pkt = respQueue.front();
  uint64_t now = getTick();

  --dram_pkt->rankRef.readEntries;

  assert(dram_pkt->rankRef.outstandingEvents > 0);
  --dram_pkt->rankRef.outstandingEvents;

  assert((dram_pkt->rankRef.pwrState != PowerState::SelfRefresh) &&
         (dram_pkt->rankRef.pwrState != PowerState::PrechargePowerdown) &&
         (dram_pkt->rankRef.pwrState != PowerState::ActivePowerdown));

  if (dram_pkt->rankRef.isQueueEmpty() &&
      dram_pkt->rankRef.outstandingEvents == 0 && gem5Config->enablePowerdown) {
    assert(!isScheduled(dram_pkt->rankRef.activateEvent));
    assert(!isScheduled(dram_pkt->rankRef.prechargeEvent));

    PowerState next_pwr_state = PowerState::ActivePowerdown;
    if (dram_pkt->rankRef.pwrState == PowerState::Idle) {
      next_pwr_state = PowerState::PrechargePowerdown;
    }

    dram_pkt->rankRef.powerDownSleep(next_pwr_state, now);
  }

  if (dram_pkt->burstHelper) {
    dram_pkt->burstHelper->burstsServiced++;
    if (dram_pkt->burstHelper->burstsServiced ==
        dram_pkt->burstHelper->burstCount) {
      accessAndRespond(
          dram_pkt, gem5Config->frontendLatency + gem5Config->backendLatency);

      delete dram_pkt->burstHelper;
      dram_pkt->burstHelper = nullptr;
    }
  }
  else {
    accessAndRespond(dram_pkt,
                     gem5Config->frontendLatency + gem5Config->backendLatency);
  }

  delete respQueue.front();
  respQueue.pop_front();

  if (!respQueue.empty()) {
    assert(respQueue.front()->readyTime >= now);
    assert(!isScheduled(respondEvent));

    scheduleAbs(respondEvent, 0ull, respQueue.front()->readyTime);
  }

  if (retryRdReq) {
    retryRdReq = false;

    retryRead();
  }
}

TimingDRAM::DRAMPacketQueue::iterator TimingDRAM::chooseNext(
    DRAMPacketQueue &queue, uint64_t extra_col_delay) {
  auto ret = queue.end();

  if (!queue.empty()) {
    if (queue.size() == 1) {
      DRAMPacket *dram_pkt = *(queue.begin());

      if (ranks[dram_pkt->rank]->inRefIdleState()) {
        ret = queue.begin();
      }
    }
    else if (gem5Config->scheduling == Config::MemoryScheduling::FCFS) {
      // check if there is a packet going to a free rank
      for (auto i = queue.begin(); i != queue.end(); ++i) {
        DRAMPacket *dram_pkt = *i;
        if (ranks[dram_pkt->rank]->inRefIdleState()) {
          ret = i;
          break;
        }
      }
    }
    else if (gem5Config->scheduling == Config::MemoryScheduling::FRFCFS) {
      ret = chooseNextFRFCFS(queue, extra_col_delay);
    }
    else {
      panic("No scheduling policy chosen");
    }
  }
  return ret;
}

TimingDRAM::DRAMPacketQueue::iterator TimingDRAM::chooseNextFRFCFS(
    DRAMPacketQueue &queue, uint64_t extra_col_delay) {
  vector<uint32_t> earliest_banks(pStructure->burst, 0);

  bool filled_earliest_banks = false;
  bool hidden_bank_prep = false;
  bool found_hidden_bank = false;
  bool found_prepped_pkt = false;
  bool found_earliest_pkt = false;

  auto selected_pkt_it = queue.end();

  const uint64_t min_col_at = MAX(nextBurstAt + extra_col_delay, getTick());

  for (auto i = queue.begin(); i != queue.end(); ++i) {
    DRAMPacket *dram_pkt = *i;

    const Bank &bank = dram_pkt->bankRef;
    const uint64_t col_allowed_at =
        dram_pkt->isRead() ? bank.rdAllowedAt : bank.wrAllowedAt;

    if (dram_pkt->rankRef.inRefIdleState()) {
      if (bank.openRow == dram_pkt->row) {
        if (col_allowed_at <= min_col_at) {
          selected_pkt_it = i;

          break;
        }
        else if (!found_hidden_bank && !found_prepped_pkt) {
          selected_pkt_it = i;
          found_prepped_pkt = true;
        }
      }
      else if (!found_earliest_pkt) {
        if (!filled_earliest_banks) {
          std::tie(earliest_banks, hidden_bank_prep) =
              minBankPrep(queue, min_col_at);
          filled_earliest_banks = true;
        }

        if (bits(earliest_banks[dram_pkt->rank], dram_pkt->bank,
                 dram_pkt->bank)) {
          found_earliest_pkt = true;
          found_hidden_bank = hidden_bank_prep;

          if (hidden_bank_prep || !found_prepped_pkt)
            selected_pkt_it = i;
        }
      }
    }
  }

  return selected_pkt_it;
}

void TimingDRAM::accessAndRespond(DRAMPacket *dram_pkt,
                                  uint64_t static_latency) {
  if (dram_pkt->eid != InvalidEventID) {
    scheduleRel(dram_pkt->eid, dram_pkt->data, static_latency);
  }

  return;
}

void TimingDRAM::activateBank(Rank &rank_ref, Bank &bank_ref, uint64_t act_tick,
                              uint32_t row) {
  assert(rank_ref.actTicks.size() == pStructure->activationLimit);

  assert(bank_ref.openRow == Bank::NO_ROW);
  bank_ref.openRow = row;
  bank_ref.bytesAccessed = 0;
  bank_ref.rowAccesses = 0;

  ++rank_ref.numBanksActive;
  assert(rank_ref.numBanksActive <= pStructure->bank);

  rank_ref.cmdList.emplace_back(
      Command(Data::MemCommand::ACT, bank_ref.bank, act_tick));

  bank_ref.preAllowedAt = act_tick + pTiming->tRAS;
  bank_ref.rdAllowedAt = MAX(act_tick + pTiming->tRCD, bank_ref.rdAllowedAt);
  bank_ref.wrAllowedAt = MAX(act_tick + pTiming->tRCD, bank_ref.wrAllowedAt);

  for (int i = 0; i < pStructure->bank; i++) {
    if (bankGroupArch && (bank_ref.bankgr == rank_ref.banks[i].bankgr)) {
      rank_ref.banks[i].actAllowedAt =
          MAX(act_tick + pTiming->tRRD_L, rank_ref.banks[i].actAllowedAt);
    }
    else {
      rank_ref.banks[i].actAllowedAt =
          MAX(act_tick + pTiming->tRRD, rank_ref.banks[i].actAllowedAt);
    }
  }

  if (!rank_ref.actTicks.empty()) {
    panic_if(rank_ref.actTicks.back() &&
                 (act_tick - rank_ref.actTicks.back()) < pTiming->tXAW,
             "Got %d activates in window %d (%llu - %llu) which "
             "is smaller than %llu",
             pStructure->activationLimit, act_tick - rank_ref.actTicks.back(),
             act_tick, rank_ref.actTicks.back(), pTiming->tXAW);

    rank_ref.actTicks.pop_back();
    rank_ref.actTicks.push_front(act_tick);

    if (rank_ref.actTicks.back() &&
        (act_tick - rank_ref.actTicks.back()) < pTiming->tXAW) {
      for (int j = 0; j < pStructure->bank; j++)
        rank_ref.banks[j].actAllowedAt =
            MAX(rank_ref.actTicks.back() + pTiming->tXAW,
                rank_ref.banks[j].actAllowedAt);
    }
  }

  if (!isScheduled(rank_ref.activateEvent)) {
    scheduleAbs(rank_ref.activateEvent, 0ull, act_tick);
  }
  else if (object.cpu->when(rank_ref.activateEvent) > act_tick) {
    deschedule(rank_ref.activateEvent);
    scheduleAbs(rank_ref.activateEvent, 0ull, act_tick);
  }
}

void TimingDRAM::prechargeBank(Rank &rank_ref, Bank &bank, uint64_t pre_at,
                               bool trace) {
  assert(bank.openRow != Bank::NO_ROW);

  uint64_t pre_done_at = pre_at + pTiming->tRP;

  bank.openRow = Bank::NO_ROW;
  bank.preAllowedAt = pre_at;
  bank.actAllowedAt = MAX(bank.actAllowedAt, pre_done_at);

  assert(rank_ref.numBanksActive != 0);
  --rank_ref.numBanksActive;

  if (trace) {
    rank_ref.cmdList.emplace_back(
        Command(Data::MemCommand::PRE, bank.bank, pre_at));
  }

  if (!isScheduled(rank_ref.prechargeEvent)) {
    scheduleAbs(rank_ref.prechargeEvent, 0ull, pre_done_at);
    ++rank_ref.outstandingEvents;
  }
  else if (object.cpu->when(rank_ref.prechargeEvent) > pre_done_at) {
    deschedule(rank_ref.prechargeEvent);
    scheduleAbs(rank_ref.prechargeEvent, 0ull, pre_done_at);
  }
}

void TimingDRAM::doDRAMAccess(DRAMPacket *dram_pkt) {
  uint64_t now = getTick();
  Rank &rank = dram_pkt->rankRef;

  if (rank.inLowPowerState) {
    assert(rank.pwrState != PowerState::SelfRefresh);
    rank.scheduleWakeUpEvent(pTiming->tXP);
  }

  Bank &bank = dram_pkt->bankRef;

  if (bank.openRow != dram_pkt->row) {
    if (bank.openRow != Bank::NO_ROW) {
      prechargeBank(rank, bank, MAX(bank.preAllowedAt, now));
    }

    uint64_t act_tick = MAX(bank.actAllowedAt, now);

    activateBank(rank, bank, act_tick, dram_pkt->row);
  }

  const uint64_t col_allowed_at =
      dram_pkt->isRead() ? bank.rdAllowedAt : bank.wrAllowedAt;
  uint64_t cmd_at = std::max({col_allowed_at, nextBurstAt, now});

  dram_pkt->readyTime = cmd_at + pTiming->tCL + pTiming->tBURST;

  uint64_t dly_to_rd_cmd;
  uint64_t dly_to_wr_cmd;

  for (int j = 0; j < pStructure->burst; j++) {
    for (int i = 0; i < pStructure->bank; i++) {
      if (dram_pkt->rank == j) {
        if (bankGroupArch && (bank.bankgr == ranks[j]->banks[i].bankgr)) {
          dly_to_rd_cmd = dram_pkt->isRead() ? pTiming->tCCD_L
                                             : MAX(pTiming->tCCD_L, wrToRdDly);
          dly_to_wr_cmd = dram_pkt->isRead() ? MAX(pTiming->tCCD_L, rdToWrDly)
                                             : pTiming->tCCD_L_WR;
        }
        else {
          dly_to_rd_cmd = dram_pkt->isRead() ? pTiming->tBURST : wrToRdDly;
          dly_to_wr_cmd = dram_pkt->isRead() ? rdToWrDly : pTiming->tBURST;
        }
      }
      else {
        dly_to_wr_cmd = rankToRankDly;
        dly_to_rd_cmd = rankToRankDly;
      }

      ranks[j]->banks[i].rdAllowedAt =
          MAX(cmd_at + dly_to_rd_cmd, ranks[j]->banks[i].rdAllowedAt);
      ranks[j]->banks[i].wrAllowedAt =
          MAX(cmd_at + dly_to_wr_cmd, ranks[j]->banks[i].wrAllowedAt);
    }
  }

  activeRank = dram_pkt->rank;

  bank.preAllowedAt =
      MAX(bank.preAllowedAt, dram_pkt->isRead()
                                 ? cmd_at + pTiming->tRTP
                                 : dram_pkt->readyTime + pTiming->tWR);

  bank.bytesAccessed += burstSize;
  ++bank.rowAccesses;

  bool auto_precharge = gem5Config->policy == Config::PagePolicy::Close ||
                        bank.rowAccesses == gem5Config->maxAccessesPerRow;

  if (!auto_precharge &&
      (gem5Config->policy == Config::PagePolicy::OpenAdaptive ||
       gem5Config->policy == Config::PagePolicy::CloseAdaptive)) {
    bool got_more_hits = false;
    bool got_bank_conflict = false;

    auto &queue = dram_pkt->isRead() ? readQueue : writeQueue;
    auto p = queue.begin();

    while (!got_more_hits && p != queue.end()) {
      if (dram_pkt != (*p)) {
        bool same_rank_bank =
            (dram_pkt->rank == (*p)->rank) && (dram_pkt->bank == (*p)->bank);

        bool same_row = dram_pkt->row == (*p)->row;
        got_more_hits |= same_rank_bank && same_row;
        got_bank_conflict |= same_rank_bank && !same_row;
      }
      ++p;
    }

    auto_precharge = !got_more_hits &&
                     (got_bank_conflict ||
                      gem5Config->policy == Config::PagePolicy::CloseAdaptive);
  }

  Data::MemCommand::cmds command =
      dram_pkt->isRead() ? Data::MemCommand::RD : Data::MemCommand::WR;

  nextBurstAt = cmd_at + pTiming->tBURST;

  dram_pkt->rankRef.cmdList.emplace_back(
      Command(command, dram_pkt->bank, cmd_at));

  if (auto_precharge) {
    prechargeBank(rank, bank, MAX(now, bank.preAllowedAt));
  }

  nextReqTime = nextBurstAt - (pTiming->tRP + pTiming->tRCD);

  if (dram_pkt->isRead()) {
    ++readsThisTime;

    stats.bytesReadDRAM += burstSize;
    stats.totMemAccLat += dram_pkt->readyTime - dram_pkt->entryTime;
    stats.totBusLat += pTiming->tBURST;
    stats.totQLat += cmd_at - dram_pkt->entryTime;
  }
  else {
    ++writesThisTime;

    stats.bytesWritten += burstSize;
  }
}

void TimingDRAM::processNextReqEvent() {
  bool switched_cmd_type = (busState != busStateNext);

  if (switched_cmd_type) {
    if (busState == BusState::Read) {
      readsThisTime = 0;
    }
    else {
      writesThisTime = 0;
    }
  }

  busState = busStateNext;

  int busyRanks = 0;

  for (auto r : ranks) {
    if (!r->inRefIdleState()) {
      if (r->pwrState != PowerState::SelfRefresh) {
        r->checkDrainDone();
      }

      if ((r->pwrState == PowerState::SelfRefresh) && r->inLowPowerState) {
        if (r->forceSelfRefreshExit()) {
          r->scheduleWakeUpEvent(pTiming->tXS);
        }
      }
    }
  }

  if (busyRanks == pStructure->burst) {
    return;
  }

  if (busState == BusState::Read) {
    bool switch_to_writes = false;

    if (totalReadQueueSize == 0) {
      if (!(totalWriteQueueSize == 0)) {
        switch_to_writes = true;
      }
      else {
        return;
      }
    }
    else {
      bool read_found = false;
      DRAMPacketQueue::iterator to_read;

      to_read = chooseNext(readQueue, switched_cmd_type ? pTiming->tCS : 0);

      if (to_read != readQueue.end()) {
        read_found = true;
      }

      if (!read_found) {
        return;
      }

      auto dram_pkt = *to_read;

      assert(dram_pkt->rankRef.inRefIdleState());

      doDRAMAccess(dram_pkt);

      ++dram_pkt->rankRef.outstandingEvents;

      assert(dram_pkt->size <= burstSize);
      assert(dram_pkt->readyTime >= getTick());

      logResponse(BusState::Read, 1);

      if (respQueue.empty()) {
        assert(!isScheduled(respondEvent));
        scheduleAbs(respondEvent, 0ull, dram_pkt->readyTime);
      }
      else {
        assert(respQueue.back()->readyTime <= dram_pkt->readyTime);
        assert(isScheduled(respondEvent));
      }

      respQueue.emplace_back(dram_pkt);

      if (totalWriteQueueSize > writeHighThreshold) {
        switch_to_writes = true;
      }

      readQueue.erase(to_read);
    }

    if (switch_to_writes) {
      busStateNext = BusState::Write;
    }
  }
  else {
    bool write_found = false;
    DRAMPacketQueue::iterator to_write;

    to_write = chooseNext(
        writeQueue, switched_cmd_type ? MIN(pTiming->tRTW, pTiming->tCS) : 0);

    if (to_write != writeQueue.end()) {
      write_found = true;
    }

    if (!write_found) {
      return;
    }

    auto dram_pkt = *to_write;

    assert(dram_pkt->rankRef.inRefIdleState());
    assert(dram_pkt->size <= burstSize);

    doDRAMAccess(dram_pkt);

    --dram_pkt->rankRef.writeEntries;

    if (!isScheduled(dram_pkt->rankRef.writeDoneEvent)) {
      scheduleAbs(dram_pkt->rankRef.writeDoneEvent, 0ull, dram_pkt->readyTime);
      ++dram_pkt->rankRef.outstandingEvents;
    }
    else if (object.cpu->when(dram_pkt->rankRef.writeDoneEvent) <
             dram_pkt->readyTime) {
      deschedule(dram_pkt->rankRef.writeDoneEvent);
      scheduleAbs(dram_pkt->rankRef.writeDoneEvent, 0ull, dram_pkt->readyTime);
    }

    isInWriteQueue.erase(burstAlign(dram_pkt->addr));

    logResponse(BusState::Write, 1);

    writeQueue.erase(to_write);

    delete dram_pkt;

    if (totalWriteQueueSize == 0 ||
        (totalReadQueueSize && writesThisTime >= gem5Config->minWriteBurst)) {
      busStateNext = BusState::Read;
    }
  }

  if (!isScheduled(nextReqEvent))
    scheduleAbs(nextReqEvent, 0ull, MAX(nextReqTime, getTick()));

  if (retryWrReq && totalWriteQueueSize < gem5Config->writeBufferSize) {
    retryWrReq = false;

    retryWrite();
  }
}

pair<vector<uint32_t>, bool> TimingDRAM::minBankPrep(
    const DRAMPacketQueue &queue, uint64_t min_col_at) {
  uint64_t min_act_at = std::numeric_limits<uint64_t>::max();
  uint64_t now = getTick();

  vector<uint32_t> bank_mask(pStructure->burst, 0);

  const uint64_t hidden_act_max = MAX(min_col_at - pTiming->tRCD, now);
  bool found_seamless_bank = false;
  bool hidden_bank_prep = false;
  vector<bool> got_waiting(pStructure->burst * pStructure->bank, false);

  for (const auto &p : queue) {
    if (p->rankRef.inRefIdleState())
      got_waiting[p->bankId] = true;
  }

  for (int i = 0; i < pStructure->burst; i++) {
    for (int j = 0; j < pStructure->bank; j++) {
      uint16_t bank_id = i * pStructure->bank + j;

      if (got_waiting[bank_id]) {
        assert(ranks[i]->inRefIdleState());

        uint64_t act_at =
            ranks[i]->banks[j].openRow == Bank::NO_ROW
                ? MAX(ranks[i]->banks[j].actAllowedAt, now)
                : MAX(ranks[i]->banks[j].preAllowedAt, now) + pTiming->tRP;

        const uint64_t col_allowed_at = (busState == BusState::Read)
                                            ? ranks[i]->banks[j].rdAllowedAt
                                            : ranks[i]->banks[j].wrAllowedAt;
        uint64_t col_at = MAX(col_allowed_at, act_at + pTiming->tRCD);
        bool new_seamless_bank = col_at <= min_col_at;

        if (new_seamless_bank ||
            (!found_seamless_bank && act_at <= min_act_at)) {
          if (!found_seamless_bank &&
              (new_seamless_bank || act_at < min_act_at)) {
            std::fill(bank_mask.begin(), bank_mask.end(), 0);
          }

          found_seamless_bank |= new_seamless_bank;

          hidden_bank_prep = act_at <= hidden_act_max;

          replaceBits(bank_mask[i], j, j, 1);
          min_act_at = act_at;
        }
      }
    }
  }

  return make_pair(bank_mask, hidden_bank_prep);
}

void TimingDRAM::logRequest(BusState dir, uint64_t entries) {
  if (dir == BusState::Read) {
    totalReadQueueSize += entries;
  }
  else {
    totalWriteQueueSize += entries;
  }
}

void TimingDRAM::logResponse(BusState dir, uint64_t entries) {
  if (dir == BusState::Read) {
    totalReadQueueSize -= entries;
  }
  else {
    totalWriteQueueSize -= entries;
  }
}

void TimingDRAM::retryRead() {
  while (retryReadQueue.size() > 0) {
    auto &req = retryReadQueue.front();
    auto ret = receive(req.addr, req.size, true, req.eid, req.data);

    if (retryRdReq) {
      break;
    }
    else if (ret) {
      retryReadQueue.pop_front();
    }
    else {
      scheduleNow(req.eid, req.data);
    }
  }
}

void TimingDRAM::retryWrite() {
  while (retryWriteQueue.size() > 0) {
    auto &req = retryWriteQueue.front();
    receive(req.addr, req.size, false, req.eid, req.data);

    if (!retryWrReq) {
      scheduleNow(req.eid, req.data);

      retryWriteQueue.pop_front();
    }
    else {
      break;
    }
  }
}

void TimingDRAM::read(uint64_t addr, uint64_t size, Event eid, uint64_t data) {
  auto ret = receive(addr, (uint32_t)size, true, eid, data);

  if (!ret) {
    // Not submitted
    if (retryRdReq) {
      // Put request to retry queue
      retryReadQueue.emplace_back(
          RetryRequest(addr, (uint32_t)size, eid, data));
    }
    else {
      // Read served by write queue
      scheduleNow(eid, data);
    }
  }
}

void TimingDRAM::write(uint64_t addr, uint64_t size, Event eid, uint64_t data) {
  receive(addr, (uint32_t)size, false, eid, data);

  if (retryWrReq) {
    // Put request to retry queue
    retryWriteQueue.emplace_back(RetryRequest(addr, (uint32_t)size, eid, data));
  }
  else {
    // Request successfully pushed to queue (write is async!)
    scheduleNow(eid, data);
  }
}

uint64_t TimingDRAM::allocate(uint64_t size) {
  uint64_t ret = 0;
  uint64_t unallocated = capacity;

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

void TimingDRAM::getStatList(std::vector<Stat> &list,
                             std::string prefix) noexcept {
  auto size = ranks.size();

  stats.getStatList(list, prefix + ".dram");

  for (uint32_t i = 0; i < size; i++) {
    ranks.at(i)->getStatList(list, prefix + ".dram.rank" + std::to_string(i));
  }
}

void TimingDRAM::getStatValues(std::vector<double> &values) noexcept {
  uint64_t duration = getTick() - lastStatsResetTick;

  stats.getStatValues(values, (double)duration / 1000000000000.);

  for (auto &iter : ranks) {
    iter->getStatValues(values);
  }
}

void TimingDRAM::resetStatValues() noexcept {
  stats.resetStatValues();

  for (auto &iter : ranks) {
    iter->resetStatValues();
  }
}

void TimingDRAM::backupQueue(std::ostream &out,
                             const DRAMPacketQueue *queue) const {
  bool exist;
  uint64_t size = queue->size();
  BACKUP_SCALAR(out, size);

  for (auto &iter : *queue) {
    BACKUP_SCALAR(out, iter->entryTime);
    BACKUP_SCALAR(out, iter->readyTime);
    BACKUP_SCALAR(out, iter->read);
    BACKUP_SCALAR(out, iter->rank);
    BACKUP_SCALAR(out, iter->bank);
    BACKUP_SCALAR(out, iter->row);
    BACKUP_SCALAR(out, iter->bankId);
    BACKUP_SCALAR(out, iter->addr);
    BACKUP_SCALAR(out, iter->size);
    BACKUP_EVENT(out, iter->eid);
    BACKUP_SCALAR(out, iter->data);

    exist = iter->burstHelper != nullptr;
    BACKUP_SCALAR(out, exist);

    if (exist) {
      BACKUP_SCALAR(out, iter->burstHelper->burstCount);
      BACKUP_SCALAR(out, iter->burstHelper->burstsServiced);
    }
  }
}

void TimingDRAM::restoreQueue(std::istream &in, DRAMPacketQueue *queue) {
  bool exist;
  uint64_t size;
  uint64_t entryTime;
  uint64_t readyTime;
  bool read;
  uint8_t rank;
  uint8_t bank;
  uint32_t row;
  uint16_t bankId;
  uint64_t addr;
  uint32_t _size;
  Event eid;
  uint64_t data;
  uint32_t count;
  uint32_t served;

  RESTORE_SCALAR(in, size);

  for (uint64_t i = 0; i < size; i++) {
    RESTORE_SCALAR(in, entryTime);
    RESTORE_SCALAR(in, readyTime);
    RESTORE_SCALAR(in, read);
    RESTORE_SCALAR(in, rank);
    RESTORE_SCALAR(in, bank);
    RESTORE_SCALAR(in, row);
    RESTORE_SCALAR(in, bankId);
    RESTORE_SCALAR(in, addr);
    RESTORE_SCALAR(in, _size);
    RESTORE_EVENT(in, eid);
    RESTORE_SCALAR(in, data);

    auto *pkt = new DRAMPacket(entryTime, read, rank, bank, row, bankId, addr,
                               _size, ranks[rank]->banks[bank], *ranks[rank]);

    pkt->readyTime = readyTime;
    pkt->eid = eid;
    pkt->data = data;

    RESTORE_SCALAR(in, exist);

    if (exist) {
      RESTORE_SCALAR(in, count);
      RESTORE_SCALAR(in, served);

      BurstHelper *ptr = new BurstHelper(count);
      ptr->burstsServiced = served;
      pkt->burstHelper = ptr;
    }

    queue->emplace_back(pkt);
  }
}

void TimingDRAM::createCheckpoint(std::ostream &out) const noexcept {
  BACKUP_SCALAR(out, rowsPerBank);
  BACKUP_SCALAR(out, writesThisTime);
  BACKUP_SCALAR(out, readsThisTime);
  BACKUP_SCALAR(out, capacity);
  BACKUP_SCALAR(out, retryRdReq);
  BACKUP_SCALAR(out, retryWrReq);
  BACKUP_SCALAR(out, nextBurstAt);
  BACKUP_SCALAR(out, prevArrival);
  BACKUP_SCALAR(out, nextReqTime);
  BACKUP_SCALAR(out, activeRank);
  BACKUP_SCALAR(out, timestampOffset);
  BACKUP_SCALAR(out, lastStatsResetTick);
  BACKUP_SCALAR(out, busState);
  BACKUP_SCALAR(out, busStateNext);
  BACKUP_SCALAR(out, totalReadQueueSize);
  BACKUP_SCALAR(out, totalWriteQueueSize);
  BACKUP_EVENT(out, nextReqEvent);
  BACKUP_EVENT(out, respondEvent);

  uint64_t size = ranks.size();
  BACKUP_SCALAR(out, size);

  for (auto &iter : ranks) {
    iter->createCheckpoint(out);
  }

  backupQueue(out, &readQueue);
  backupQueue(out, &writeQueue);
  backupQueue(out, &respQueue);

  size = addressMap.size();
  BACKUP_SCALAR(out, size);

  for (auto &iter : addressMap) {
    BACKUP_SCALAR(out, iter.first);
    BACKUP_SCALAR(out, iter.second);
  }

  size = isInWriteQueue.size();
  BACKUP_SCALAR(out, size);

  for (auto &iter : isInWriteQueue) {
    BACKUP_SCALAR(out, iter);
  }

  stats.createCheckpoint(out);
}

void TimingDRAM::restoreCheckpoint(std::istream &in) noexcept {
  RESTORE_SCALAR(in, rowsPerBank);
  RESTORE_SCALAR(in, writesThisTime);
  RESTORE_SCALAR(in, readsThisTime);
  RESTORE_SCALAR(in, capacity);
  RESTORE_SCALAR(in, retryRdReq);
  RESTORE_SCALAR(in, retryWrReq);
  RESTORE_SCALAR(in, nextBurstAt);
  RESTORE_SCALAR(in, prevArrival);
  RESTORE_SCALAR(in, nextReqTime);
  RESTORE_SCALAR(in, activeRank);
  RESTORE_SCALAR(in, timestampOffset);
  RESTORE_SCALAR(in, lastStatsResetTick);
  RESTORE_SCALAR(in, busState);
  RESTORE_SCALAR(in, busStateNext);
  RESTORE_SCALAR(in, totalReadQueueSize);
  RESTORE_SCALAR(in, totalWriteQueueSize);
  RESTORE_EVENT(in, nextReqEvent);
  RESTORE_EVENT(in, respondEvent);

  uint64_t size;
  RESTORE_SCALAR(in, size);

  for (uint64_t i = 0; i < size; i++) {
    ranks.at(i)->restoreCheckpoint(in);
  }

  restoreQueue(in, &readQueue);
  restoreQueue(in, &writeQueue);
  restoreQueue(in, &respQueue);

  RESTORE_SCALAR(in, size);

  addressMap.reserve(size);

  for (uint64_t i = 0; i < size; i++) {
    uint64_t f, s;

    RESTORE_SCALAR(in, f);
    RESTORE_SCALAR(in, s);

    addressMap.emplace_back(std::make_pair(f, s));
  }

  RESTORE_SCALAR(in, size);

  isInWriteQueue.reserve(size);

  for (uint64_t i = 0; i < size; i++) {
    uint64_t t;

    RESTORE_SCALAR(in, t);

    isInWriteQueue.emplace(t);
  }

  stats.restoreCheckpoint(in);
}

}  // namespace SimpleSSD::Memory::DRAM
