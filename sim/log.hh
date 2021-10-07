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

const std::string idPrefix[] = {
    "global",  //!< DebugID::Common
    "CPU",
    "Memory",
    "Memory::DRAM",
    "Memory::SRAM",
    "HIL",
    "HIL::Common",
    "HIL::NVMe",
    "HIL::NVMe::Command",
    "ICL",
    "ICL::GenericManager",
    "ICL::RingBuffer",
    "ICL::SetAssociative",
    "FTL",
    "FTL::Mapping::PageLevelMapping",
    "FTL::GC::NaiveGC",
    "FTL::GC::AdvancedGC",
    "FTL::GC::PreemptibleGC",
    "FTL::ReadReclaim::BasicReadReclaim",
    "FIL",
    "FIL::PALOLD",
};

const std::string logPrefix[] = {
    "info",   //!< LogID::Info
    "warn",   //!< LogID::Warn
    "panic",  //!< LogID::Panic
};

// Fancy printer for template parameter pack with c-style format string
class Printer {
 private:
  const std::ios::fmtflags mask = std::ios::basefield | std::ios::adjustfield |
                                  std::ios::floatfield | std::ios::boolalpha |
                                  std::ios::showbase | std::ios::showpos |
                                  std::ios::uppercase;

  std::ostream *os;
  const char *end;
  char *cur;

  /* Old formats */

  std::streamsize width;
  std::streamsize precision;
  std::ios::fmtflags flags;

  /* For printing */

  union {
    uint32_t data;
    struct {
      bool argwidth : 1;      // '*' at width
      bool argprecision : 1;  // '*' at precision
      bool err : 1;  // Error while parsing format, #args not matched, and so on
      bool intAsChar : 1;
      bool charAsInt : 1;
      bool pad : 3;
      int8_t consume;
      uint16_t pad2;
    };
  };

  std::ptrdiff_t parseFormat();

  inline void clearFormat() {
    os->width(width);
    os->precision(precision);
    os->setf(flags, mask);
    os->fill(' ');

    argwidth = false;
    argprecision = false;
    consume = 0;
  }

  inline void changeWidth(const char *str) {
    if (UNLIKELY(str != nullptr)) {
      if (UNLIKELY(str[0] == '*')) {
        argwidth = true;
        consume++;
      }
      else {
        auto n = strtoul(str, nullptr, 10);

        width = os->width(n);
      }
    }
  }

  inline void changePrecision(const char *str) {
    if (UNLIKELY(str != nullptr)) {
      if (UNLIKELY(str[0] == '*')) {
        argprecision = true;
        consume++;
      }
      else {
        auto n = MIN(strtoul(str, nullptr, 10), 1ul);

        precision = os->precision(n);
      }
    }
  }

  inline void changeFlag(std::ios::fmtflags f) { flags = os->setf(f, mask); }

  inline void checkSign(const char *begin, const char *end,
                        std::ios::fmtflags &f) {
    bool minus = false;
    bool plus = false;
    bool zero = false;
    bool sharp = false;

    for (char *c = const_cast<char *>(begin); c < end; c++) {
      switch (*c) {
        case '-':
          minus = true;
          break;
        case '+':
          plus = true;
          break;
        case '0':
          zero = true;
          break;
        case '#':
          sharp = true;
          break;
        default:
          break;
      }
    }

    if (UNLIKELY(minus)) {
      f |= std::ios::left;
    }
    if (UNLIKELY(plus)) {
      f |= std::ios::showpos;
    }
    if (UNLIKELY(zero)) {
      os->fill('0');
    }
    if (UNLIKELY(sharp)) {
      f |= std::ios::showbase;
    }
  }

  template <class T, std::enable_if_t<std::is_integral_v<T>, bool> = true>
  inline uint16_t get(const T &value) {
    return static_cast<uint16_t>(value);
  }

  template <class T, std::enable_if_t<!std::is_integral_v<T>, bool> = true>
  inline uint16_t get(const T &) {
    err = true;

    return 1;
  }

 public:
  Printer(std::ostream *, const char *);
  ~Printer();

  inline void flush() {
    for (; cur < end; cur++) {
      if (UNLIKELY(*cur == '%')) {
        cur += parseFormat();

        if (UNLIKELY(consume < 0)) {
          // % does not consume args
          consume = 0;

          continue;
        }

        err = true;
      }

      os->put(*cur);
    }

    if (UNLIKELY(err)) {
      (*os) << " [Format Error]";
    }
  }

  template <class T, std::enable_if_t<!std::is_enum_v<T>, bool> = true>
  void printImpl(const T &value) {
    if constexpr (std::is_integral_v<T>) {
      if (intAsChar && std::is_integral_v<T>) {
        (*os) << (char)value;
      }
      else if (charAsInt && std::is_same_v<T, char>) {
        (*os) << (int16_t)value;
      }
      else if (charAsInt && std::is_same_v<T, uint8_t>) {
        (*os) << (uint16_t)value;
      }
      else {
        (*os) << value;
      }
    }
    else {
      (*os) << value;
    }
  }

  template <class T, std::enable_if_t<std::is_enum_v<T>, bool> = true>
  void printImpl(const T &value) {
    (*os) << static_cast<uint64_t>(value);
  }

  template <class T>
  Printer &operator<<(const T &value) {
    if (UNLIKELY(cur == end)) {
      err = true;
    }

    // From the cur, print until % met
    for (; cur < end; cur++) {
      if (UNLIKELY(*cur == '%')) {
        cur += parseFormat();

        if (UNLIKELY(consume < 0)) {
          // % does not consume args
          consume = 0;

          continue;
        }
        else if (UNLIKELY(consume > 0)) {
          consume--;

          if (argwidth) {
            argwidth = false;
            width = os->width(get<T>(value));
          }
          else if (argprecision) {
            argprecision = false;
            precision = os->precision(get<T>(value));
          }

          continue;
        }

        printImpl(value);
        clearFormat();

        break;
      }
      else {
        os->put(*cur);
      }
    }

    return *this;
  }
};

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
    Memory,
    Memory_DRAM,
    Memory_SRAM,
    HIL,
    HIL_Common,
    HIL_NVMe,
    HIL_NVMe_Command,
    ICL,
    ICL_BasicManager,
    ICL_RingBuffer,
    ICL_SetAssociative,
    FTL,
    FTL_PageLevel,
    FTL_NaiveGC,
    FTL_AdvancedGC,
    FTL_PreemptibleGC,
    FTL_BasicReadReclaim,
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

  template <class... T>
  void print(LogID id, const char *format, T &&... args) const noexcept {
    std::ostream *stream = nullptr;

    if (UNLIKELY(!inited)) {
      std::cerr << "panic: Log system not initialized" << std::endl;

      abort();
    }

    switch (id) {
      case LogID::Info:
        stream = out;

        break;
      case LogID::Warn:
      case LogID::Panic:
        stream = err;

        break;
      default:
        std::cerr << "panic: Undefined Log ID: " << (uint32_t)id << std::endl;

        abort();

        break;
    }

    if (UNLIKELY(!stream)) {
      return;
    }

    if (LIKELY(stream->good())) {
      *stream << cpu->getTick() << ": " << logPrefix[(uint32_t)id] << ": ";

      {
        Printer printer(stream, format);

        (printer << ... << args).flush();
      }

      *stream << std::endl;
    }
    else {
      std::cerr << "panic: Stream is not opened" << std::endl;

      abort();
    }

    if (id == LogID::Panic) {
      abort();
    }
  }

  template <class... T>
  void debugprint(DebugID id, const char *format, T &&... args) const noexcept {
    if (UNLIKELY(!inited)) {
      std::cerr << "panic: Log system not initialized" << std::endl;

      abort();
    }

    if (UNLIKELY(!debug)) {
      return;
    }

    if (LIKELY(debug->good())) {
      *debug << cpu->getTick() << ": " << idPrefix[(uint32_t)id] << ": ";

      {
        Printer printer(debug, format);

        (printer << ... << args).flush();
      }

      *debug << std::endl;
    }
    else {
      std::cerr << "panic: debugfile is not opened" << std::endl;

      abort();
    }
  }
};

}  // namespace SimpleSSD

#endif
