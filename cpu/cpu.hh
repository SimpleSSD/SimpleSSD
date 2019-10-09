// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_CPU_CPU__
#define __SIMPLESSD_CPU_CPU__

#include <map>

#include "sim/config_reader.hh"
#include "sim/engine.hh"
#include "sim/object.hh"
#include "util/sorted_map.hh"

namespace SimpleSSD::CPU {

enum class CPUGroup {
  HostInterface,          //!< Assign event to HIL core
  InternalCache,          //!< Assign event to ICL core
  FlashTranslationLayer,  //!< Assign event to FTL core
  Any,                    //!< Assign event to most-idle core
};

/**
 * \brief CPU object declaration
 *
 * This object manages event scheduling and firmware execution latency.
 */
class CPU {
 private:
  struct EventData {
    EventFunction func;
    std::string name;

    EventData() {}
    EventData(EventFunction &f, std::string &s) : func(f), name(s) {}
    EventData(const EventData &) = delete;
    EventData(EventData &&) noexcept = default;

    EventData &operator=(const EventData &) = delete;
    EventData &operator=(EventData &&) noexcept = default;
  };

  Engine *engine;        //!< Simulation engine
  ConfigReader *config;  //!< Config reader
  Log *log;              //!< Log engine

  uint16_t hilCore;
  uint16_t iclCore;
  uint16_t ftlCore;

  std::map<uint64_t, Event> *coreList;
  std::map<Event, EventData> eventList;

  void dispatch();

 public:
  CPU(Engine *, ConfigReader *);
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
   * \brief Schedule event
   *
   * If event was previously scheduled, the event will be called twice.
   * NOT RESCHEDULE THE EVENT.
   *
   * As we cannot use void* context because of checkpointing, it become hard to
   * exchange data between event. You may create data queue for these purpose.
   *
   * \param[in] group CPU group to execute event (and add-up instruction stats)
   * \param[in] eid   Event ID to schedule
   * \param[in] delay Delay in pico-second
   */
  void schedule(CPUGroup group, Event eid, uint64_t delay) noexcept;

  /**
   * \brief Deschedule event
   *
   * Deschedule event.
   *
   * \param[in] eid Event ID to deschedule
   * \param[in] all Deschedule all scheduled events.
   */
  void deschedule(Event eid, bool all = false) noexcept;

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

  void getStatList(std::vector<Object::Stat> &, std::string) noexcept;
  void getStatValues(std::vector<double> &) noexcept;
  void resetStatValues() noexcept;

  void createCheckpoint(std::ostream &) const noexcept;
  void restoreCheckpoint(std::istream &) noexcept;
};

}  // namespace SimpleSSD::CPU

#endif
