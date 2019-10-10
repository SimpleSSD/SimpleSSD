// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_CPU_CPU__
#define __SIMPLESSD_CPU_CPU__

#include <cstdarg>
#include <map>

#include "lib/mcpat/mcpat.h"
#include "sim/config_reader.hh"
#include "sim/engine.hh"

namespace SimpleSSD {

class Log;

struct Stat {
  std::string name;
  std::string desc;
};

namespace CPU {

class CPU;

enum class CPUGroup {
  None,                   //!< Only for events, not function
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

  uint64_t sum();
  void clear();
};

/**
 * \brief CPU object declaration
 *
 * This object manages event scheduling and firmware execution latency.
 */
class CPU {
 private:
  class EventData {
   public:
    EventFunction func;
    std::string name;

    EventData() {}
    EventData(EventFunction &f, std::string &s) : func(f), name(s) {}
    EventData(const EventData &) = delete;
    EventData(EventData &&) noexcept = default;

    EventData &operator=(const EventData &) = delete;
    EventData &operator=(EventData &&) = default;
  };

  class EventStat {
   public:
    uint64_t handledFunction;
    uint64_t handledEvent;
    uint64_t busy;

    EventStat() : handledFunction(0), handledEvent(0), busy(0) {}

    void clear() {
      handledFunction = 0;
      handledEvent = 0;
      busy = 0;
    }
  };

  class Core {
   public:
    class Job {
     public:
      Event eid;
      EventData *data;

      Job() : eid(InvalidEventID), data(nullptr) {}
    };

    EventStat eventStat;
    Function instructionStat;

    std::map<uint64_t, Job> eventQueue;

    uint64_t busyUntil;

    Core() : busyUntil(0) {}
  };

  Engine *engine;        //!< Simulation engine
  ConfigReader *config;  //!< Config reader
  Log *log;              //!< Log engine

  uint64_t lastResetStat;

  uint64_t clockSpeed;
  uint64_t clockPeriod;

  bool useDedicatedCore;
  uint16_t hilCore;
  uint16_t iclCore;
  uint16_t ftlCore;

  uint64_t curTick;

  std::vector<Core> coreList;
  std::map<Event, EventData> eventList;

  void updateSchedule();
  void calculatePower(Power &);
  Core *getIdleCoreInRange(uint16_t, uint16_t);

  void dispatch(uint64_t);
  void interrupt(Event, uint64_t);

  inline void panic_log(const char *format, ...) noexcept;

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
   * Create event. You must create all events in the constructor of object. If
   * not, you must care checkpointing on your own.
   *
   * \param[in] func  Callback function when event is triggered
   * \param[in] name  Description of the event
   * \return Event ID
   */
  Event createEvent(EventFunction func, std::string name) noexcept;

  /**
   * \brief Schedule function
   *
   * If event was previously scheduled, the event will be called twice.
   * NOT RESCHEDULE THE EVENT.
   *
   * As we cannot use void* context because of checkpointing, it become hard to
   * exchange data between event. You may create data queue for these purpose.
   *
   * \param[in] group CPU group to execute event (and add-up instruction stats)
   * \param[in] eid   Event ID to schedule
   * \param[in] func  Instruction and cycle info
   */
  void schedule(CPUGroup group, Event eid, const Function &func) noexcept;

  /**
   * \brief Schedule event
   *
   * This is short-hand schedule function when we just need to call event
   * immediately. It does not affects CPU statistic object.
   *
   * \param[in] eid   Event ID to schedule
   * \param[in] delay Ticks to delay
   */
  void schedule(Event eid, uint64_t delay = 0) noexcept;

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
};

}  // namespace CPU

}  // namespace SimpleSSD

#endif
