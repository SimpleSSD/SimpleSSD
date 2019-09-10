// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIM_log_HH__
#define __SIM_log_HH__

#include <cinttypes>
#include <cstdarg>
#include <fstream>

namespace SimpleSSD {

class Engine;

/**
 * \brief Log object declaration
 *
 * Log object for logging and assertion.
 */
class Log {
 public:
  enum class ID : uint32_t {
    Common,
  };

 private:
  Engine *engine;  //!< Simulation engine
  bool inited;     //!< Flag whether this object is initialized

  std::ofstream *out;    //!< File for info
  std::ofstream *err;    //!< File for warn/panic
  std::ofstream *debug;  //!< File for debug printout

  bool checkStandardIO(std::ofstream &) noexcept;
  void print(std::ofstream &, const char *, va_list) noexcept;

 public:
  Log(Engine *);
  Log(const Log &) = delete;
  Log(Log &&) = default;
  ~Log();

  Log &operator=(const Log &) = delete;
  Log &operator=(Log &&) noexcept = default;

  void init(std::ofstream &, std::ofstream &, std::ofstream &) noexcept;
  void deinit() noexcept;

  void panic(const char *, ...) noexcept;
  void warn(const char *, ...) noexcept;
  void info(const char *, ...) noexcept;
  void debugprint(ID, const char *, ...) noexcept;
};

}  // namespace SimpleSSD

#endif
