// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "sim/log.hh"

#include <iostream>
#include <vector>

#include "sim/engine.hh"
#include "util/algorithm.hh"

namespace SimpleSSD {

const std::string idPrefix[] = {
    "global",  //!< DebugID::Common
    "CPU",
    "DRAM",
    "DRAM::TimingDRAM",
    "SRAM",
    "HIL",
    "HIL::Common",
    "HIL::NVMe",
    "HIL::NVMe::Command",
    "ICL",
    "ICL::RingBuffer",
    "FTL",
    "FTL::Mapping::PageLevel",
    "FTL::*::VirtuallyLinked",
    "FIL",
    "FIL::PALOLD",
};

const std::string logPrefix[] = {
    "info",   //!< LogID::Info
    "warn",   //!< LogID::Warn
    "panic",  //!< LogID::Panic
};

//! A constructor
Log::Log() : inited(false), out(nullptr), err(nullptr), debug(nullptr) {}

//! A destructor
Log::~Log() {
  if (inited) {
    deinit();
  }
}

/**
 * \brief Initialize log system
 *
 * Initialize log system by provided file stream.
 * All std::ofstream objects should be opened.
 *
 * \param[in]  cpu       Pointer to CPU object
 * \param[in]  outfile   std::ofstream object for output file
 * \param[in]  errfile   std::ofstream object for error file
 * \param[in]  debugfile std::ofstream object for debug log file
 */
void Log::init(CPU::CPU *c, std::ostream *outfile, std::ostream *errfile,
               std::ostream *debugfile) noexcept {
  cpu = c;

  if (UNLIKELY(outfile == nullptr || errfile == nullptr ||
               debugfile == nullptr)) {
    std::cerr << "panic: Got null-pointer" << std::endl;

    abort();
  }

  if (UNLIKELY(!outfile->good())) {
    // We don't have panic yet
    std::cerr << "panic: outfile is not opened" << std::endl;

    abort();
  }

  if (UNLIKELY(!errfile->good())) {
    std::cerr << "panic: errfile is not opened" << std::endl;

    abort();
  }

  if (UNLIKELY(!debugfile->good())) {
    std::cerr << "panic: debugfile is not opened" << std::endl;

    abort();
  }

  out = outfile;
  err = errfile;
  debug = debugfile;

  inited = true;
}

//! Deinitialize log system
void Log::deinit() noexcept {
  if (!checkStandardIO(out)) {
    ((std::ofstream *)out)->close();
  }
  if (!checkStandardIO(err)) {
    ((std::ofstream *)err)->close();
  }
  if (!checkStandardIO(debug)) {
    ((std::ofstream *)debug)->close();
  }

  inited = false;
}

//! Print log and terminate program
void Log::print(LogID id, const char *format, va_list args) noexcept {
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

  if (LIKELY(err->good())) {
    *stream << cpu->getTick() << ": " << logPrefix[(uint32_t)id] << ": ";

    print(stream, format, args);

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

void Log::debugprint(DebugID id, const char *format, va_list args) noexcept {
  if (UNLIKELY(!inited)) {
    std::cerr << "panic: Log system not initialized" << std::endl;

    abort();
  }

  if (LIKELY(debug->good())) {
    *debug << cpu->getTick() << ": " << idPrefix[(uint32_t)id] << ": ";

    print(debug, format, args);

    *debug << std::endl;
  }
  else {
    std::cerr << "panic: debugfile is not opened" << std::endl;

    abort();
  }
}

/**
 * Check file stream is stdout or stderr
 *
 * \param[in]  os std::ofstream object to check
 * \return True if os is stdout or stderr
 */
bool Log::checkStandardIO(std::ostream *os) noexcept {
  if (os->rdbuf() == std::cout.rdbuf() || os->rdbuf() == std::cerr.rdbuf()) {
    return true;
  }

  return false;
}

/**
 * Print log to file stream
 *
 * \param[in]  os     std::ofstream object to print
 * \param[in]  format Format string
 * \param[in]  args   Argument list
 */
void Log::print(std::ostream *os, const char *format, va_list args) noexcept {
  va_list copy;
  std::vector<char> str;

  va_copy(copy, args);
  str.resize(vsnprintf(nullptr, 0, format, copy) + 1);
  va_end(copy);

  vsnprintf(str.data(), str.size(), format, args);

  *os << str.data();
}

}  // namespace SimpleSSD
