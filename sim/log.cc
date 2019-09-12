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

namespace SimpleSSD {

const std::string idPrefix[] = {
    "global",  //!< ID::Common
};

//! A constructor
Log::Log(Engine *e)
    : engine(e), inited(false), out(nullptr), err(nullptr), debug(nullptr) {}

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
 * \param[in]  outfile    std::ofstream object for output file
 * \param[in]  errfile    std::ofstream object for error file
 * \param[out] debugfile std::ofstream object for debug log file
 */
void Log::init(std::ofstream &outfile, std::ofstream &errfile,
               std::ofstream &debugfile) noexcept {
  if (!outfile.is_open()) {
    // We don't have panic yet
    std::cerr << "panic: outfile is not opened" << std::endl;

    abort();
  }

  if (!errfile.is_open()) {
    std::cerr << "panic: errfile is not opened" << std::endl;

    abort();
  }

  if (!debugfile.is_open()) {
    std::cerr << "panic: debugfile is not opened" << std::endl;

    abort();
  }

  out = &outfile;
  err = &errfile;
  debug = &debugfile;

  inited = true;
}

//! Deinitialize log system
void Log::deinit() noexcept {
  if (out->is_open() && !checkStandardIO(*out)) {
    out->close();
  }
  if (err->is_open() && !checkStandardIO(*err)) {
    err->close();
  }
  if (debug->is_open() && !checkStandardIO(*debug)) {
    debug->close();
  }

  inited = false;
}

//! Print log and terminate program
void Log::panic(const char *format, ...) noexcept {
  if (err->is_open()) {
    va_list args;

    *err << engine->getTick() << ": panic: ";

    va_start(args, format);
    print(*err, format, args);
    va_end(args);

    *err << std::endl;
  }
  else {
    std::cerr << "panic: errfile is not opened" << std::endl;
  }

  abort();
}

//! Print log
void Log::warn(const char *format, ...) noexcept {
  if (err->is_open()) {
    va_list args;

    *err << engine->getTick() << ": warn: ";

    va_start(args, format);
    print(*err, format, args);
    va_end(args);

    *err << std::endl;
  }
  else {
    std::cerr << "panic: errfile is not opened" << std::endl;

    abort();
  }
}

//! Print log
void Log::info(const char *format, ...) noexcept {
  if (out->is_open()) {
    va_list args;

    *out << engine->getTick() << ": warn: ";

    va_start(args, format);
    print(*out, format, args);
    va_end(args);

    *out << std::endl;
  }
  else {
    std::cerr << "panic: outfile is not opened" << std::endl;

    abort();
  }
}

void Log::debugprint(ID id, const char *format, ...) noexcept {
  if (debug->is_open()) {
    va_list args;

    *debug << engine->getTick() << idPrefix[id] << ": warn: ";

    va_start(args, format);
    print(*debug, format, args);
    va_end(args);

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
bool Log::checkStandardIO(std::ofstream &os) noexcept {
  if (os.rdbuf() == std::cout.rdbuf() || os.rdbuf() == std::cerr.rdbuf()) {
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
void Log::print(std::ofstream &os, const char *format, va_list args) noexcept {
  va_list copy;
  std::vector<char> str;

  va_copy(copy, args);
  str.resize(vsnprintf(nullptr, 0, format, copy) + 1);
  va_end(copy);

  vsnprintf(str.data(), str.size(), format, args);

  os << str.data();
}

}  // namespace SimpleSSD
