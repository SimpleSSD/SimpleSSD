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
 *          Matthias Jung
 *          Wendy Elsasser
 *          Radhika Jagtap
 */

#pragma once

#ifndef __SIMPLESSD_MEM_DRAM_GEM5_HH__
#define __SIMPLESSD_MEM_DRAM_GEM5_HH__

#include <deque>
#include <unordered_set>
#include <vector>

#include "mem/dram/abstract_dram.hh"

namespace SimpleSSD::Memory::DRAM {

enum class PowerState : uint8_t {
  Idle,                //!< Precharged
  Refresh,             //!< Auto refresh
  SelfRefresh,         //!< Self refresh
  PrechargePowerdown,  //!< Precharge power down
  Active,              //!< Row active
  ActivePowerdown,     //!< Active power down
};

enum class RefreshState : uint8_t {
  Idle,
  Drain,
  ExitPowerdown,    //!< Evaluate power state and issue wakeup
  ExitSelfRefresh,  //!< Exiting self refresh
  Precharge,        //!< Close all open banks
  Start,            //!< Refresh start
  Run,              //!< Refresh running
};

enum class BusState : uint8_t {
  Read,
  Write,
};

class TimingDRAM;
class Rank;

/**
 * \brief Wrapper class of libDRAMPower by gem5
 */
class DRAMPower {
 private:
  static Data::MemArchitectureSpec getArchParams(ConfigReader *);
  static Data::MemTimingSpec getTimingParams(ConfigReader *);
  static Data::MemPowerSpec getPowerParams(ConfigReader *);
  static uint8_t getDataRate(ConfigReader *);
  static bool hasTwoVDD(ConfigReader *);
  static Data::MemorySpecification getMemSpec(ConfigReader *);

 public:
  libDRAMPower powerlib;

  DRAMPower(ConfigReader *, bool);
};

struct Command {
  Data::MemCommand::cmds type;
  uint8_t bank;
  uint64_t timestamp;

  constexpr Command(Data::MemCommand::cmds _type, uint8_t _bank,
                    uint64_t time_stamp)
      : type(_type), bank(_bank), timestamp(time_stamp) {}
};

class Bank {
 public:
  static const uint32_t NO_ROW = -1;

  uint32_t openRow;
  uint8_t bank;
  uint8_t bankgr;

  uint64_t rdAllowedAt;
  uint64_t wrAllowedAt;
  uint64_t preAllowedAt;
  uint64_t actAllowedAt;

  uint32_t rowAccesses;
  uint32_t bytesAccessed;

  Bank(ObjectData &)
      : openRow(NO_ROW),
        bank(0),
        bankgr(0),
        rdAllowedAt(0),
        wrAllowedAt(0),
        preAllowedAt(0),
        actAllowedAt(0),
        rowAccesses(0),
        bytesAccessed(0) {}

  void createCheckpoint(std::ostream &) const noexcept;
  void restoreCheckpoint(std::istream &) noexcept;
};

class RankStats {
 private:
  friend Rank;

  Rank *parent;

  double actEnergy;
  double preEnergy;
  double readEnergy;
  double writeEnergy;
  double refreshEnergy;
  double actBackEnergy;
  double preBackEnergy;
  double actPowerDownEnergy;
  double prePowerDownEnergy;
  double selfRefreshEnergy;
  double totalEnergy;
  double averagePower;
  double totalIdleTime;

 public:
  RankStats(Rank *);

  void getStatList(std::vector<Stat> &, std::string) noexcept;
  void getStatValues(std::vector<double> &) noexcept;
  void resetStatValues() noexcept;

  void createCheckpoint(std::ostream &) const noexcept;
  void restoreCheckpoint(std::istream &) noexcept;
};

class Rank : public Object {
 private:
  friend TimingDRAM;

  TimingDRAM *parent;

  Config::DRAMTiming *timing;
  Config::TimingDRAMConfig *gem5Config;

  PowerState pwrStateTrans;
  PowerState pwrStatePostRefresh;

  uint64_t pwrStateTick;
  uint64_t refreshDueAt;

  PowerState pwrState;
  RefreshState refreshState;

  bool inLowPowerState;
  uint8_t rank;
  uint32_t readEntries;
  uint32_t writeEntries;

  uint8_t outstandingEvents;

  uint64_t wakeUpAllowedAt;

  DRAMPower power;

  std::vector<Command> cmdList;
  std::vector<Bank> banks;

  uint32_t numBanksActive;

  std::deque<uint64_t> actTicks;

  RankStats stats;

  Event writeDoneEvent;
  Event activateEvent;
  Event prechargeEvent;
  Event refreshEvent;
  Event powerEvent;
  Event wakeUpEvent;

  void updatePowerStats();
  void schedulePowerEvent(PowerState, uint64_t);

  void processWriteDoneEvent();
  void processActivateEvent();
  void processPrechargeEvent();
  void processRefreshEvent();
  void processPowerEvent();
  void processWakeUpEvent();

 public:
  Rank(ObjectData &, TimingDRAM *, uint8_t);

  void startup(uint64_t);
  void suspend();
  bool inRefIdleState() const { return refreshState == RefreshState::Idle; }
  bool inPwrIdleState() const { return pwrState == PowerState::Idle; }
  bool forceSelfRefreshExit() const;
  bool isQueueEmpty() const;
  void checkDrainDone();
  void flushCmdList();
  void computeStats();
  void powerDownSleep(PowerState, uint64_t);
  void scheduleWakeUpEvent(uint64_t);

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

class BurstHelper {
 public:
  const uint32_t burstCount;
  uint32_t burstsServiced;

  BurstHelper(uint32_t _burstCount)
      : burstCount(_burstCount), burstsServiced(0) {}
};

class DRAMPacket {
 private:
  friend TimingDRAM;

  const uint64_t entryTime;
  uint64_t readyTime;

  const bool read;

  const uint8_t rank;
  const uint8_t bank;
  const uint32_t row;

  const uint16_t bankId;

  uint64_t addr;
  uint32_t size;

  BurstHelper *burstHelper;
  Bank &bankRef;
  Rank &rankRef;

  Event eid;
  uint64_t data;

 public:
  DRAMPacket(uint64_t now, bool is_read, uint8_t _rank, uint8_t _bank,
             uint32_t _row, uint16_t bank_id, uint64_t _addr,
             unsigned int _size, Bank &bank_ref, Rank &rank_ref)
      : entryTime(now),
        readyTime(now),
        read(is_read),
        rank(_rank),
        bank(_bank),
        row(_row),
        bankId(bank_id),
        addr(_addr),
        size(_size),
        burstHelper(nullptr),
        bankRef(bank_ref),
        rankRef(rank_ref),
        eid(InvalidEventID),
        data(0) {}

  inline bool isRead() const { return read; }
};

class DRAMStats {
 private:
  friend TimingDRAM;

  TimingDRAM *parent;

  double readReqs;
  double writeReqs;
  double readBursts;
  double writeBursts;
  double servicedByWrQ;
  double mergedWrBursts;
  double neitherReadNorWriteReqs;

  double totQLat;
  double totBusLat;
  double totMemAccLat;
  double avgQLat;
  double avgBusLat;
  double avgMemAccLat;
  double numRdRetry;
  double numWrRetry;
  double bytesReadDRAM;
  double bytesReadWrQ;
  double bytesWritten;

  double avgRdBW;
  double avgWrBW;
  double peakBW;
  double busUtil;
  double busUtilRead;
  double busUtilWrite;

  double totGap;
  double avgGap;

 public:
  DRAMStats(TimingDRAM *);

  void getStatList(std::vector<Stat> &, std::string) noexcept;
  void getStatValues(std::vector<double> &, double) noexcept;
  void resetStatValues() noexcept;

  void createCheckpoint(std::ostream &) const noexcept;
  void restoreCheckpoint(std::istream &) noexcept;
};

/**
 * \brief gem5 Timing CPU DRAM controller model
 *
 * Ports of gem5's DRAM controller when using Timing (TimingSimpleCPU, InOrder,
 * HPI, Out-of-order CPU). This class has own DRAMPower library - not using
 * base class's DRAMPower calculation.
 */
class TimingDRAM : public AbstractDRAM {
 private:
  struct RetryRequest {
    uint64_t addr;
    uint32_t size;
    Event eid;
    uint64_t data;

    RetryRequest(uint64_t a, uint32_t s, Event e, uint64_t d)
        : addr(a), size(s), eid(e), data(d) {}
  };

  friend Rank;
  friend DRAMStats;

  using DRAMPacketQueue = std::deque<DRAMPacket *>;

  Config::TimingDRAMConfig *gem5Config;

  const uint32_t burstSize;
  const uint32_t rowBufferSize;
  const uint32_t columnsPerRowBuffer;
  const uint32_t columnsPerStripe;
  const bool bankGroupArch;
  const uint32_t writeHighThreshold;
  const uint32_t writeLowThreshold;

  const uint64_t rankToRankDly;
  const uint64_t wrToRdDly;
  const uint64_t rdToWrDly;

  uint32_t rowsPerBank;
  uint32_t writesThisTime;
  uint32_t readsThisTime;

  uint64_t capacity;
  std::vector<std::pair<uint64_t, uint64_t>> addressMap;

  bool retryRdReq;
  bool retryWrReq;

  DRAMPacketQueue readQueue;
  DRAMPacketQueue writeQueue;

  std::unordered_set<uint64_t> isInWriteQueue;

  DRAMPacketQueue respQueue;

  std::vector<Rank *> ranks;

  uint64_t nextBurstAt;
  uint64_t prevArrival;
  uint64_t nextReqTime;

  DRAMStats stats;

  uint8_t activeRank;

  uint64_t timestampOffset;
  uint64_t lastStatsResetTick;

  BusState busState;
  BusState busStateNext;

  uint64_t totalReadQueueSize;
  uint64_t totalWriteQueueSize;

  std::deque<RetryRequest> retryReadQueue;
  std::deque<RetryRequest> retryWriteQueue;

  Event nextReqEvent;
  Event respondEvent;

  void processNextReqEvent();
  void processRespondEvent();

  bool readQueueFull(uint32_t) const;
  bool writeQueueFull(uint32_t) const;

  bool addToReadQueue(uint64_t, uint32_t, uint32_t, Event, uint64_t);
  bool addToWriteQueue(uint64_t, uint32_t, uint32_t);

  void doDRAMAccess(DRAMPacket *);
  void accessAndRespond(DRAMPacket *, uint64_t);

  DRAMPacket *decodeAddr(uint64_t, uint32_t, bool);
  DRAMPacketQueue::iterator chooseNext(DRAMPacketQueue &, uint64_t);
  DRAMPacketQueue::iterator chooseNextFRFCFS(DRAMPacketQueue &, uint64_t);

  std::pair<std::vector<uint32_t>, bool> minBankPrep(const DRAMPacketQueue &,
                                                     uint64_t);

  void activateBank(Rank &, Bank &, uint64_t, uint32_t);
  void prechargeBank(Rank &, Bank &, uint64_t, bool = true);

  uint64_t burstAlign(uint64_t addr) const {
    return (addr & ~((uint64_t)burstSize - 1));
  }

  void updatePowerStats(Rank &);
  static bool sortTime(const Command &cmd, const Command &cmd_next) {
    return cmd.timestamp < cmd_next.timestamp;
  };

  bool receive(uint64_t, uint32_t, bool, Event, uint64_t);

  void logRequest(BusState, uint64_t);
  void logResponse(BusState, uint64_t);

  void backupQueue(std::ostream &, const DRAMPacketQueue *) const;
  void restoreQueue(std::istream &, DRAMPacketQueue *);

  void retryRead();
  void retryWrite();

 public:
  TimingDRAM(ObjectData &);

  void read(uint64_t, uint64_t, Event, uint64_t) override;
  void write(uint64_t, uint64_t, Event, uint64_t) override;

  uint64_t allocate(uint64_t) override;

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::Memory::DRAM

#endif
