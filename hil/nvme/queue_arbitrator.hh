// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_HIL_NVME_QUEUE_ARBITRATOR_HH__
#define __SIMPLESSD_HIL_NVME_QUEUE_ARBITRATOR_HH__

#include <functional>
#include <queue>

#include "hil/nvme/queue.hh"
#include "sim/abstract_subsystem.hh"
#include "util/sorted_map.hh"

namespace SimpleSSD::HIL::NVMe {

class ControllerData;

class SQContext {
 private:
  friend class CQContext;
  friend class Arbitrator;

  SQEntry entry;

  uint16_t commandID;
  uint16_t sqID;
  uint16_t cqID;
  uint16_t sqHead;

  bool useSGL;
  bool aborted;
  bool dispatched;
  bool completed;

 public:
  SQContext();

  //! Update field with provided entry data + parameters.
  void update(uint16_t, uint16_t, uint16_t) noexcept;
  void update() noexcept;

  //! Get internal SQEntry data buffer
  SQEntry *getData() noexcept;

  // Getter
  uint32_t getID() noexcept;
  uint16_t getSQID() noexcept;
  uint16_t getCQID() noexcept;
  bool isSGL() noexcept;
  bool isAbort() noexcept;

  // Setter
  void abort() noexcept;
};

class CQContext {
 private:
  friend class Arbitrator;

  CQEntry entry;

  // uint16_t commandID;  //!< commandID at entry.dword3
  // uint16_t sqID;  //!< sqID at entry.dword2
  uint16_t cqID;

 public:
  CQContext();

  void update(SQContext *) noexcept;

  template <
      class Type,
      std::enable_if_t<std::conjunction_v<
                           std::is_enum<Type>,
                           std::is_same<std::underlying_type_t<Type>, uint8_t>>,
                       Type> = Type()>
  void makeStatus(bool dnr, bool more, StatusType sct, Type sc) noexcept {
    entry.dword3.status = 0;  // Phase field will be filled before DMA

    entry.dword3.dnr = dnr ? 1 : 0;
    entry.dword3.more = more ? 1 : 0;
    entry.dword3.sct = (uint8_t)sct;
    entry.dword3.sc = (uint8_t)sc;
  }

  bool isSuccess() noexcept;

  CQEntry *getData();

  uint32_t getID() noexcept;
  uint16_t getSQID() noexcept;
  uint16_t getCQID() noexcept;
};

union ArbitrationData {
  uint32_t data;
  struct {
    uint8_t ab : 3;
    uint8_t rsvd : 5;
    uint8_t lpw;
    uint8_t mpw;
    uint8_t hpw;
  };
};

class Arbitrator : public Object {
 public:
  using InterruptFunction = std::function<void(uint16_t, bool)>;

 private:
  ControllerData *controller;
  ControllerID controllerID;

  Event work;

  // Work params
  uint64_t period;
  uint64_t internalQueueSize;
  uint64_t lastInvokedAt;

  // Queue
  uint16_t cqSize;
  uint16_t sqSize;
  CQueue **cqList;
  SQueue **sqList;

  // WRR params (See Set/Get Features > Arbitration)
  Arbitration mode;
  ArbitrationData param;

  // Internal queue (Indexed by commandID, sorted by insertion order)
  unordered_map_queue requestQueue;
  unordered_map_queue dispatchedQueue;
  std::queue<CQContext *> completionQueue;

  // Completion
  Event eventCompDone;
  void completion_done();

  // Shutdown
  bool shutdownReserved;

  // Work
  bool run;
  bool running;
  std::queue<SQContext *> collectQueue;

  Event eventCollect;
  void collect_done();

  bool checkQueue(uint16_t);
  bool collectRoundRobin();
  bool collectWeightedRoundRobin();

  void collect(uint64_t);

 public:
  Arbitrator(ObjectData &, ControllerData *);
  ~Arbitrator();

  // Register
  void enable(bool);
  void setMode(Arbitration);
  void ringSQ(uint16_t, uint16_t);
  void ringCQ(uint16_t, uint16_t);
  void reserveShutdown();
  void createAdminSQ(uint64_t, uint16_t, Event);
  void createAdminCQ(uint64_t, uint16_t, Event);

  // Command
  SQContext *dispatch();
  void complete(CQContext *, bool = false);

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;

  // Return restored SQContext
  SQContext *getRecoveredRequest(uint32_t);
};

}  // namespace SimpleSSD::HIL::NVMe

#endif
