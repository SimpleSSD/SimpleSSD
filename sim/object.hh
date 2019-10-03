// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIM_OBJECT_HH__
#define __SIM_OBJECT_HH__

#include "sim/checkpoint.hh"
#include "sim/config_reader.hh"
#include "sim/engine.hh"
#include "sim/log.hh"
#include "util/algorithm.hh"

#ifndef __FILENAME__
#define __FILENAME__ __FILE__
#endif

namespace SimpleSSD {

#define panic_if(cond, format, ...)                                            \
  {                                                                            \
    if (UNLIKELY(cond)) {                                                      \
      panic_log("%s:%s: %s:" format, __FILENAME__, __LINE__, __FUNCTION__,     \
                ##__VA_ARGS__);                                                \
    }                                                                          \
  }

#define panic(format, ...)                                                     \
  {                                                                            \
    panic_log("%s:%s: %s:" format, __FILENAME__, __LINE__, __FUNCTION__,       \
              ##__VA_ARGS__);                                                  \
  }

#define warn_if(cond, format, ...)                                             \
  {                                                                            \
    if (UNLIKELY(cond)) {                                                      \
      warn_log("%s:%s: %s:" format, __FILENAME__, __LINE__, __FUNCTION__,      \
               ##__VA_ARGS__);                                                 \
    }                                                                          \
  }

#define warn(format, ...)                                                      \
  {                                                                            \
    warn_log("%s:%s: %s:" format, __FILENAME__, __LINE__, __FUNCTION__,        \
             ##__VA_ARGS__);                                                   \
  }

#define info(format, ...)                                                      \
  {                                                                            \
    info_log("%s:%s: %s:" format, __FILENAME__, __LINE__, __FUNCTION__,        \
             ##__VA_ARGS__);                                                   \
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

  inline ObjectData makeObject() { return ObjectData(engine, config, log); }

  /* Helper APIs for Engine */
  inline uint64_t getTick() noexcept { return engine->getTick(); }
  inline Event createEvent(EventFunction ef, std::string s) noexcept {
    return engine->createEvent(ef, s);
  }
  inline void schedule(Event e, uint64_t t, EventContext c) noexcept {
    engine->schedule(e, t, c);
  }
  inline void schedule(Event e, uint64_t t) noexcept {
    engine->schedule(e, t, EventContext());
  }
  inline void deschedule(Event e) noexcept { engine->deschedule(e); }
  inline bool isScheduled(Event e) noexcept { return engine->isScheduled(e); }
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
  inline void info_log(const char *format, ...) noexcept {
    va_list args;

    va_start(args, format);
    log->print(Log::LogID::Info, format, args);
    va_end(args);
  }
  inline void warn_log(const char *format, ...) noexcept {
    va_list args;

    va_start(args, format);
    log->print(Log::LogID::Warn, format, args);
    va_end(args);
  }
  inline void panic_log(const char *format, ...) noexcept {
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
  Object(ObjectData &o) : engine(o.engine), config(o.config), log(o.log) {}
  virtual ~Object() {}

  /* Statistic API */
  virtual void getStatList(std::vector<Stat> &, std::string) noexcept = 0;
  virtual void getStatValues(std::vector<double> &) noexcept = 0;
  virtual void resetStatValues() noexcept = 0;

  /* Checkpoint API */
  virtual void createCheckpoint(std::ostream &) noexcept = 0;
  virtual void restoreCheckpoint(std::istream &) noexcept = 0;
};

}  // namespace SimpleSSD

#endif
