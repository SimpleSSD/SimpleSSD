// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_SIM_HARDWARE_OBJECT_HH__
#define __SIMPLESSD_SIM_HARDWARE_OBJECT_HH__

#include "sim/config_reader.hh"
#include "sim/log.hh"

namespace SimpleSSD {

/**
 * \brief Common data for Object (Hardware)
 *
 * Hardware simulation models are hard to inherit SimpleSSD::Object.
 * Instead, inherit this class.
 */
class HardwareObject {
 protected:
  ConfigReader *config;
  Log *log;

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
  inline bool writeConfigInt(Section s, uint32_t k, int64_t v) noexcept {
    return config->writeInt(s, k, v);
  }
  inline bool writeConfigUint(Section s, uint32_t k, uint64_t v) noexcept {
    return config->writeUint(s, k, v);
  }
  inline bool writeConfigFloat(Section s, uint32_t k, float v) noexcept {
    return config->writeFloat(s, k, v);
  }
  inline bool writeConfigString(Section s, uint32_t k,
                                std::string &v) noexcept {
    return config->writeString(s, k, v);
  }
  inline bool writeConfigBoolean(Section s, uint32_t k, bool v) noexcept {
    return config->writeBoolean(s, k, v);
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
  HardwareObject(ConfigReader *c, Log *l) : config(c), log(l) {}
  HardwareObject(const HardwareObject &) = delete;
  HardwareObject(HardwareObject &&) noexcept = delete;
  virtual ~HardwareObject() {}

  HardwareObject &operator=(const HardwareObject &) = delete;
  HardwareObject &operator=(HardwareObject &&) = delete;

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
