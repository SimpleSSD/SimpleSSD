// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIM_OBJECT_HH__
#define __SIM_OBJECT_HH__

#include "sim/config.hh"
#include "sim/engine.hh"
#include "sim/log.hh"

namespace SimpleSSD {

#define panic(format, ...)                                                     \
  { log->panic(format, __VA_ARGS__); }

#define panic_if(cond, format, ...)                                            \
  {                                                                            \
    if (cond) {                                                                \
      log->panic(format, __VA_ARGS__);                                         \
    }                                                                          \
  }

#define warn(format, ...)                                                      \
  { log->warn(format, __VA_ARGS__); }

#define warn_if(cond, format, ...)                                             \
  {                                                                            \
    if (cond) {                                                                \
      log->warn(format, __VA_ARGS__);                                          \
    }                                                                          \
  }

#define info(format, ...)                                                      \
  { log->info(format, __VA_ARGS__); }

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
  Engine *engine;  //!< Current simulation engine
  Config *config;  //!< Current simulation configuration
  Log *log;        //!< Current log system

  /* Helper APIs for Engine */
  inline uint64_t getTick() noexcept { return engine->getTick(); }
  inline Event createEvent(EventFunction ef, std::string s) noexcept {
    return engine->createEvent(ef, s);
  }
  inline void schedule(Event e, uint64_t t) noexcept { engine->schedule(e, t); }
  inline void deschedule(Event e) noexcept { engine->deschedule(e); }
  inline bool isScheduled(Event e) noexcept { engine->isScheduled(e); }
  inline void removeEvent(Event e) noexcept { engine->removeEvent(e); }

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

 public:
  Object(Engine *e, Config *c, Log *l) : engine(e), config(c), log(l) {}
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
