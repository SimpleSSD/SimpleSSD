// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __HIL_NVME_QUEUE_ARBITRATOR_HH__
#define __HIL_NVME_QUEUE_ARBITRATOR_HH__

#include "hil/nvme/queue.hh"
#include "sim/abstract_subsystem.hh"
#include "util/sorted_map.hh"

namespace SimpleSSD::HIL::NVMe {

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
  uint16_t getCommandID() noexcept;
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
  void makeStatus(bool, bool, StatusType, uint8_t) noexcept;

  CQEntry *getData();

  uint16_t getCommandID() noexcept;
  uint16_t getSQID() noexcept;
  uint16_t getCQID() noexcept;
};

class InterruptContext {
 public:
  uint16_t iv;
  bool post;
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
 private:
  bool inited;

  ControllerData *controller;

  Event submit;     // Arbitrator -> Subsystem (Submission)
  Event complete;   // Subsystem -> Arbitrator (Completion)
  Event interrupt;  // Arbitrator -> InterruptManager (Interrupt)
  Event work;

  // Work params
  uint64_t period;
  uint64_t internalQueueSize;

  // Queue
  uint16_t cqSize;
  uint16_t sqSize;
  CQueue **cqList;
  SQueue **sqList;

  // Interrupt context
  InterruptContext intrruptContext;

  // WRR params (See Set/Get Features > Arbitration)
  Arbitration mode;
  ArbitrationData param;

  // Internal queue (Indexed by commandID, sorted by insertion order)
  unordered_map_queue requestQueue;
  unordered_map_queue dispatchedQueue;

  // Completion
  Event eventCompDone;
  void completion_done(uint64_t, CQContext *);

  // Shutdown
  bool shutdownReserved;

  Event eventShutdown;

  // Work
  bool run;
  bool running;
  uint64_t collectRequested;
  uint64_t collectCompleted;

  Event eventCollect;
  void collect_done(uint64_t, SQContext *);

  bool checkQueue(uint16_t);
  void collectRoundRobin();
  void collectWeightedRoundRobin();

  void completion(uint64_t, CQContext *);
  void collect(uint64_t);

  // Admin commands
  // bool createSQ(SQContext *);
  // bool createCQ(SQContext *);
  // bool deleteSQ(SQContext *);
  // bool deleteCQ(SQContext *);
  // bool abort(SQContext *);
  // bool setFeature(SQContext *);
  // bool getFeature(SQContext *);

 public:
  Arbitrator(ObjectData &, ControllerData &);
  ~Arbitrator();

  /**
   * \brief Initialize Queue Arbitrator
   *
   * \param[in] s Submission Event
   * \param[in] i Interrupt Event
   * \param[in] t Shutdown Event
   * \return Completion Event
   */
  Event init(Event s, Event i, Event t);

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

  // bool createSQ(uint16_t, uint16_t, uint16_t, QueuePriority, bool, uint64_t,
  //               Event, EventContext);
  // bool createCQ(uint16_t, uint16_t, uint16_t, bool, bool, uint64_t, Event,
  //               EventContext);
  // bool deleteSQ(uint16_t);
  // bool deleteCQ(uint16_t);
  // bool submitCommand(SQContext *);

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::HIL::NVMe

#endif
