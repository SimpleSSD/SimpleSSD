// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIM_OBJECT_HH__
#define __SIM_OBJECT_HH__

#include "sim/config_reader.hh"
#include "sim/engine.hh"
#include "sim/log.hh"
#include "util/algorithm.hh"

namespace SimpleSSD {

#define panic_if(cond, format, ...)                                            \
  {                                                                            \
    if (UNLIKELY(cond)) {                                                      \
      panic(format, ##__VA_ARGS__);                                            \
    }                                                                          \
  }

#define warn_if(cond, format, ...)                                             \
  {                                                                            \
    if (UNLIKELY(cond)) {                                                      \
      warn(format, ##__VA_ARGS__);                                             \
    }                                                                          \
  }

using ObjectData = struct _ObjectData {
  Engine *engine;
  ConfigReader *config;
  Log *log;

  _ObjectData() : engine(nullptr), config(nullptr), log(nullptr) {}
  _ObjectData(Engine *e, ConfigReader *c, Log *l)
      : engine(e), config(c), log(l) {}
};

/**
 * \brief Object object declaration
 *
 * Simulation object. All simulation module must inherit this class. Provides
 * API for accessing config, engine and log system.
 */
class Object {
 public:
  struct Stat {
    std::string name;
    std::string desc;
  };

 protected:
  Engine *engine;        //!< Current simulation engine
  ConfigReader *config;  //!< Current simulation configuration
  Log *log;              //!< Current log system

  inline ObjectData &&makeObject() { return ObjectData(engine, config, log); }

  /* Helper APIs for Engine */
  inline uint64_t getTick() noexcept { return engine->getTick(); }
  inline Event createEvent(EventFunction ef, std::string s) noexcept {
    return engine->createEvent(ef, s);
  }
  inline void schedule(Event e, uint64_t t, void *c = nullptr) noexcept {
    engine->schedule(e, t, c);
  }
  inline void deschedule(Event e) noexcept { engine->deschedule(e); }
  inline bool isScheduled(Event e) noexcept { engine->isScheduled(e); }
  inline void destroyEvent(Event e) noexcept { engine->destroyEvent(e); }

  /* Helper APIs for Config */
  inline int64_t readConfigInt(Section s, uint32_t k) noexcept {
    return config->readInt(s, k);
  }
  inline uint64_t readConfigUint(Section s, uint32_t k) noexcept {
    return config->readUint(s, k);
  }
  inline float readConfigFloat(Section s, uint32_t k) noexcept {
    return config->readFloat(s, k);
  }
  inline std::string readConfigString(Section s, uint32_t k) noexcept {
    return config->readString(s, k);
  }
  inline bool readConfigBoolean(Section s, uint32_t k) noexcept {
    return config->readBoolean(s, k);
  }

  /* Helper APIs for Log */
  inline void info(const char *format, ...) noexcept {
    va_list args;

    va_start(args, format);
    log->print(Log::LogID::Info, format, args);
    va_end(args);
  }
  inline void warn(const char *format, ...) noexcept {
    va_list args;

    va_start(args, format);
    log->print(Log::LogID::Warn, format, args);
    va_end(args);
  }
  inline void panic(const char *format, ...) noexcept {
    va_list args;

    va_start(args, format);
    log->print(Log::LogID::Panic, format, args);
    va_end(args);
  }
  inline void debugprint(Log::DebugID id, const char *format, ...) noexcept {
    va_list args;

    va_start(args, format);
    log->debugprint(id, format, args);
    va_end(args);
  }

 public:
  Object() = delete;
  Object(const ObjectData &) = delete;
  Object(ObjectData &&o) noexcept
      : engine(o.engine), config(o.config), log(o.log) {}
  virtual ~Object() {}

  /* Statistic API */
  virtual void getStatList(std::vector<Stat> &, std::string) noexcept = 0;
  virtual void getStatValues(std::vector<double> &) noexcept = 0;
  virtual void resetStatValues() noexcept = 0;

  /* Checkpoint API */
  virtual void createCheckpoint() noexcept = 0;
  virtual void restoreCheckpoint() noexcept = 0;
};

}  // namespace SimpleSSD

#endif
