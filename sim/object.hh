// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_SIM_OBJECT_HH__
#define __SIMPLESSD_SIM_OBJECT_HH__

#include <vector>

#include "cpu/cpu.hh"
#include "sim/checkpoint.hh"
#include "sim/config_reader.hh"
#include "sim/log.hh"

namespace SimpleSSD {

#if !defined(NO_LOG_MACRO) && !defined(DISABLE_LOG)

#define panic_if(cond, format, ...)                                            \
  {                                                                            \
    if (UNLIKELY(cond)) {                                                      \
      panic_log("%s:%u: %s\n  " format, __FILENAME__, __LINE__,                \
                __PRETTY_FUNCTION__, ##__VA_ARGS__);                           \
    }                                                                          \
  }

#define panic(format, ...)                                                     \
  {                                                                            \
    panic_log("%s:%u: %s\n  " format, __FILENAME__, __LINE__,                  \
              __PRETTY_FUNCTION__, ##__VA_ARGS__);                             \
  }

#define warn_if(cond, format, ...)                                             \
  {                                                                            \
    if (UNLIKELY(cond)) {                                                      \
      warn_log("%s:%u: %s\n  " format, __FILENAME__, __LINE__,                 \
               __PRETTY_FUNCTION__, ##__VA_ARGS__);                            \
    }                                                                          \
  }

#define warn(format, ...)                                                      \
  {                                                                            \
    warn_log("%s:%u: %s\n  " format, __FILENAME__, __LINE__,                   \
             __PRETTY_FUNCTION__, ##__VA_ARGS__);                              \
  }

#define info(format, ...)                                                      \
  {                                                                            \
    info_log("%s:%u: %s\n  " format, __FILENAME__, __LINE__,                   \
             __PRETTY_FUNCTION__, ##__VA_ARGS__);                              \
  }

#endif

#ifdef DISABLE_LOG
#define panic_if(cond, format, ...)
#define panic(format, ...)
#define warn_if(cond, format, ...)
#define warn(format, ...)
#define info(format, ...)
#endif

using ObjectData = struct _ObjectData {
  CPU::CPU *cpu;
  ConfigReader *config;
  Log *log;

  _ObjectData() : cpu(nullptr), config(nullptr), log(nullptr) {}
  _ObjectData(CPU::CPU *e, ConfigReader *c, Log *l)
      : cpu(e), config(c), log(l) {}
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
  ObjectData object;

  /* Helper APIs for CPU */
  inline uint64_t getTick() noexcept { return object.cpu->getTick(); }
  inline Event createEvent(EventFunction ef, std::string s) noexcept {
    return object.cpu->createEvent(ef, s);
  }
  inline void schedule(CPU::CPUGroup g, Event e, uint64_t d) noexcept {
    object.cpu->schedule(g, e, d);
  }
  inline void deschedule(Event e) noexcept { object.cpu->deschedule(e); }
  inline bool isScheduled(Event e) noexcept {
    return object.cpu->isScheduled(e);
  }
  inline void destroyEvent(Event e) noexcept { object.cpu->destroyEvent(e); }

  /* Helper APIs for Config */
  inline int64_t readConfigInt(Section s, uint32_t k) noexcept {
    return object.config->readInt(s, k);
  }
  inline uint64_t readConfigUint(Section s, uint32_t k) noexcept {
    return object.config->readUint(s, k);
  }
  inline float readConfigFloat(Section s, uint32_t k) noexcept {
    return object.config->readFloat(s, k);
  }
  inline std::string readConfigString(Section s, uint32_t k) noexcept {
    return object.config->readString(s, k);
  }
  inline bool readConfigBoolean(Section s, uint32_t k) noexcept {
    return object.config->readBoolean(s, k);
  }
  inline bool writeConfigInt(Section s, uint32_t k, int64_t v) noexcept {
    return object.config->writeInt(s, k, v);
  }
  inline bool writeConfigUint(Section s, uint32_t k, uint64_t v) noexcept {
    return object.config->writeUint(s, k, v);
  }
  inline bool writeConfigFloat(Section s, uint32_t k, float v) noexcept {
    return object.config->writeFloat(s, k, v);
  }
  inline bool writeConfigString(Section s, uint32_t k,
                                std::string &v) noexcept {
    return object.config->writeString(s, k, v);
  }
  inline bool writeConfigBoolean(Section s, uint32_t k, bool v) noexcept {
    return object.config->writeBoolean(s, k, v);
  }

  /* Helper APIs for Log */
  inline void info_log(const char *format, ...) noexcept {
    va_list args;

    va_start(args, format);
    object.log->print(Log::LogID::Info, format, args);
    va_end(args);
  }
  inline void warn_log(const char *format, ...) noexcept {
    va_list args;

    va_start(args, format);
    object.log->print(Log::LogID::Warn, format, args);
    va_end(args);
  }
  inline void panic_log(const char *format, ...) noexcept {
    va_list args;

    va_start(args, format);
    object.log->print(Log::LogID::Panic, format, args);
    va_end(args);
  }
  inline void debugprint(Log::DebugID id, const char *format, ...) noexcept {
    va_list args;

    va_start(args, format);
    object.log->debugprint(id, format, args);
    va_end(args);
  }

 public:
  Object(ObjectData &o) : object(o) {}
  virtual ~Object() {}

  /* Statistic API */
  virtual void getStatList(std::vector<Stat> &, std::string) noexcept = 0;
  virtual void getStatValues(std::vector<double> &) noexcept = 0;
  virtual void resetStatValues() noexcept = 0;

  /* Checkpoint API */
  virtual void createCheckpoint(std::ostream &) const noexcept = 0;
  virtual void restoreCheckpoint(std::istream &) noexcept = 0;
};

}  // namespace SimpleSSD

#endif
