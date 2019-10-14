// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_SIM_LOG_HH__
#define __SIMPLESSD_SIM_LOG_HH__

#include <cinttypes>
#include <cstdarg>
#include <fstream>

#include "cpu/cpu.hh"

namespace SimpleSSD {

class Engine;

/**
 * \brief Log object declaration
 *
 * Log object for logging and assertion.
 */
class Log {
 public:
  enum class DebugID : uint32_t {
    Common,
    CPU,
    DRAM,
    SRAM,
    HIL,
    HIL_Common,
    HIL_NVMe,
    HIL_NVMe_Command,
    ICL,
    FTL,
    FIL,
    FIL_PALOLD,
  };

  enum class LogID : uint32_t {
    Info,
    Warn,
    Panic,
  };

 private:
  CPU::CPU *cpu;  //!< Simulation engine
  bool inited;    //!< Flag whether this object is initialized

  std::ostream *out;    //!< File for info
  std::ostream *err;    //!< File for warn/panic
  std::ostream *debug;  //!< File for debug printout

  bool checkStandardIO(std::ostream *) noexcept;
  inline void print(std::ostream *, const char *, va_list) noexcept;

 public:
  Log();
  Log(const Log &) = delete;
  Log(Log &&) noexcept = default;
  ~Log();

  Log &operator=(const Log &) = delete;
  Log &operator=(Log &&) = default;

  void init(CPU::CPU *, std::ostream *, std::ostream *,
            std::ostream *) noexcept;
  void deinit() noexcept;

  void print(LogID, const char *, va_list) noexcept;
  void debugprint(DebugID, const char *, va_list) noexcept;
};

}  // namespace SimpleSSD

#endif
