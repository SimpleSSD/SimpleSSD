// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_CPU_CPU_HH__
#define __SIMPLESSD_CPU_CPU_HH__

#include <cstdarg>
#include <deque>
#include <unordered_map>

#include "lib/mcpat/mcpat.h"
#include "sim/config_reader.hh"
#include "sim/engine.hh"
#include "util/sorted_map.hh"

namespace SimpleSSD {

class Log;

struct Stat {
  std::string name;
  std::string desc;
};

namespace CPU {

class CPU;

enum class CPUGroup {
  HostInterface,          //!< Assign function to HIL core
  InternalCache,          //!< Assign function to ICL core
  FlashTranslationLayer,  //!< Assign function to FTL core
  Any,                    //!< Assign function to most-idle core
};

class Function {
 private:
  friend CPU;

  // Instruction count
  uint64_t branch;
  uint64_t load;
  uint64_t store;
  uint64_t arithmetic;
  uint64_t floatingPoint;
  uint64_t otherInsts;

  // Cycle consumed
  uint64_t cycles;

 public:
  Function();
  Function(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t,
           uint64_t);

  Function &operator+=(const Function &);

  inline uint64_t sum();
  inline void clear();
  inline void dummy();
};

/**
 * \brief Create function object
 *
 * This will become marker for LLVM pass
 */
Function initFunction();

/**
 * \brief CPU object declaration
 *
 * This object manages event scheduling and firmware execution latency.
 */
class CPU {
 private:
  class EventStat {
   public:
    uint64_t handledFunction;
    uint64_t busy;

    EventStat() : handledFunction(0), busy(0) {}

    void clear() {
      handledFunction = 0;
      busy = 0;
    }
  };

  class Job {
   public:
    Event eid;
    uint64_t data;
    uint64_t delay;

    Job(Event e, uint64_t d) : eid(e), data(d) {}
    Job(Event e, uint64_t d, uint64_t t) : eid(e), data(d), delay(t) {}
  };

  class Core {
   private:
    CPU *parent;

    uint64_t busyUntil;
    uint64_t clockPeriod;

    EventStat eventStat;
    Function instructionStat;

    Event jobEvent;

    std::deque<Job> jobQueue;

    void handleJob(uint64_t);

   public:
    Core();

    void init(CPU *, uint32_t, uint64_t) noexcept;

    uint64_t getBusyUntil() noexcept;
    EventStat &getEventStat() noexcept;
    Function &getInstructionStat() noexcept;

    void submitJob(Event, uint64_t, uint64_t, Function &);

    void createCheckpoint(std::ostream &) const;
    void restoreCheckpoint(std::istream &);
  };

  Engine *engine;        //!< Simulation engine
  ConfigReader *config;  //!< Config reader
  Log *log;              //!< Log engine

  uint64_t lastResetStat;
  uint64_t lastScheduledAt;

  uint64_t clockSpeed;
  uint64_t clockPeriod;

  bool useDedicatedCore;
  uint16_t hilCore;
  uint16_t iclCore;
  uint16_t ftlCore;

  std::vector<Core> coreList;
  std::vector<Event> eventList;
  std::list<Job> jobQueue;

  std::unordered_map<Event, Event> oldEventList;  //!< For restoring Event

  void calculatePower(Power &);
  Core *getIdleCoreInRange(uint16_t, uint16_t);

  void dispatch(uint64_t);
  void interrupt(Event, uint64_t);

  void scheduleNext();

  inline void panic_log(const char *format, ...) noexcept;
  inline void warn_log(const char *format, ...) noexcept;

 public:
  CPU(Engine *, ConfigReader *, Log *);
  ~CPU();

  /**
   * \brief Get current simulation tick
   *
   * \return Simulation tick in pico-seconds
   */
  uint64_t getTick() noexcept;

  /**
   * \brief Create event
   *
   * Create event. You must create all events in the constructor of object.
   *
   * \param[in] func  Callback function when event is triggered
   * \param[in] name  Description of the event
   * \return Event ID
   */
  Event createEvent(EventFunction &&func, std::string &&name) noexcept;

  /**
   * \brief Schedule function
   *
   * If event was previously scheduled, the event will be rescheduled to later
   * one.
   *
   * As we cannot use void* context because of checkpointing, it become hard to
   * exchange data between event. You may create data queue for these purpose.
   *
   * \param[in] group CPU group to execute event (and add-up instruction stats)
   * \param[in] eid   Event ID to schedule
   * \param[in] func  Instruction and cycle info
   * \param[in] data  Data to pass (this should be value, not pointer)
   */
  void schedule(CPUGroup group, Event eid, uint64_t data,
                Function &func) noexcept;

  /**
   * \brief Schedule event
   *
   * This is short-hand schedule function when we just need to call event
   * immediately. It does not affects CPU statistic object.
   *
   * Use this function when calling function is not related to firmware
   * execution - not important or hardware event.
   *
   * \param[in] eid   Event ID to schedule
   * \param[in] data  Data to pass (this should be value, not pointer)
   * \param[in] delay Ticks to delay
   */
  void schedule(Event eid, uint64_t data = 0, uint64_t delay = 0) noexcept;

  /**
   * \brief Schedule event
   *
   * This is short-hand schedule function when we just need to call event
   * immediately. It does not affects CPU statistic object.
   *
   * Use this function when calling function is not related to firmware
   * execution - not important or hardware event.
   *
   * \param[in] eid   Event ID to schedule
   * \param[in] data  Data to pass (this should be value, not pointer)
   * \param[in] tick  Tick to call event
   */
  void scheduleAbs(Event eid, uint64_t data, uint64_t tick) noexcept;

  /**
   * \brief Deschedule event
   *
   * Deschedule event.
   *
   * \param[in] eid Event ID to deschedule
   */
  void deschedule(Event eid) noexcept;

  /**
   * \brief Check event is scheduled
   *
   * \param[in] eid Event ID to check
   */
  bool isScheduled(Event eid) noexcept;

  /**
   * \brief Check when event is scheduled
   *
   * \param[in] eid Event ID to check
   * \return Tick scheduled (-1 if not scheduled)
   */
  uint64_t when(Event eid) noexcept;

  /**
   * \brief Destroy event
   *
   * \param[in] eid Event ID to destroy
   */
  void destroyEvent(Event eid) noexcept;

  void getStatList(std::vector<Stat> &, std::string) noexcept;
  void getStatValues(std::vector<double> &) noexcept;
  void resetStatValues() noexcept;

  void createCheckpoint(std::ostream &) const noexcept;
  void restoreCheckpoint(std::istream &) noexcept;

  Event restoreEventID(Event) noexcept;
  void clearOldEventList() noexcept;
};

}  // namespace CPU

class EventData {
 private:
  friend CPU::CPU;

  EventFunction func;
#ifdef SIMPLESSD_DEBUG
  std::string name;
#endif

  uint64_t scheduledAt;

  inline bool isScheduled() {
    return scheduledAt != std::numeric_limits<uint64_t>::max();
  }

  inline void deschedule() {
    scheduledAt = std::numeric_limits<uint64_t>::max();
  }

 public:
#ifdef SIMPLESSD_DEBUG
  EventData(EventFunction &&f, std::string &&s)
      : func(std::move(f)),
        name(std::move(s)),
        scheduledAt(std::numeric_limits<uint64_t>::max()) {}
#else
  EventData(EventFunction &&f)
      : func(std::move(f)), scheduledAt(std::numeric_limits<uint64_t>::max()) {}
#endif
  EventData(const EventData &) = delete;
  EventData(EventData &&) noexcept = delete;

  EventData &operator=(const EventData &) = delete;
  EventData &operator=(EventData &&) = delete;
};

}  // namespace SimpleSSD

#endif
