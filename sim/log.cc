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

  out = outfile;
  err = errfile;
  debug = debugfile;

  inited = true;
}

//! Deinitialize log system
void Log::deinit() noexcept {
  inited = false;
}

//! Print log and terminate program
void Log::print(LogID id, const char *format, va_list args) const noexcept {
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

void Log::debugprint(DebugID id, const char *format, va_list args) const
    noexcept {
  if (UNLIKELY(!inited)) {
    std::cerr << "panic: Log system not initialized" << std::endl;

    abort();
  }

  if (UNLIKELY(!debug)) {
    return;
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
 * Print log to file stream
 *
 * \param[in]  os     std::ofstream object to print
 * \param[in]  format Format string
 * \param[in]  args   Argument list
 */
void Log::print(std::ostream *os, const char *format, va_list args) const
    noexcept {
  va_list copy;
  std::vector<char> str;

  va_copy(copy, args);
  str.resize(vsnprintf(nullptr, 0, format, copy) + 1);
  va_end(copy);

  vsnprintf(str.data(), str.size(), format, args);

  *os << str.data();
}

}  // namespace SimpleSSD
