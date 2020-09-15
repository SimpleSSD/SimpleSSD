// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "sim/log.hh"

#include <iostream>
#include <regex>
#include <vector>

#include "sim/engine.hh"
#include "util/algorithm.hh"

namespace SimpleSSD {

/**
 * \brief C-Style format string
 *
 * See: https://en.cppreference.com/w/cpp/io/c/fprintf
 * No reordering ('n$') supports
 */
static const std::regex regexFormat(
    "^%"                      // Format must start with %
    "((?:-|\\+| |#|0)*)"      // Sign options (only for integral types)
    "(\\*|\\d+)?"             // Width
    "(\\.)?"                  // Separater for width and precision
    "(\\*|\\d+)?"             // Precision
    "(h{1,2}|l{1,2}|j|z|t)?"  // Length modifier
    "(%|c|s|d|i|o|x|X|u|f|F|e|E|a|A|g|G|n|p)");  // Conversion specifier

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

Printer::Printer(std::ostream *o, const char *f)
    : os(o),
      end(f + strlen(f)),
      cur(const_cast<char *>(f)),
      width(0),
      precision(6),
      flags(o->setf(static_cast<std::ios::fmtflags>(0))),
      data(0) {}

Printer::~Printer() {}

std::ptrdiff_t Printer::parseFormat() {
  std::ptrdiff_t fmtlen = 0;
  std::cmatch match;

  if (std::regex_search(cur, match, regexFormat)) {
    std::ios::fmtflags f = flags;

    // Masks
    f &= ~std::ios::adjustfield;
    f &= ~std::ios::basefield;
    f &= ~std::ios::floatfield;

    // Format string length
    fmtlen = match.length();

    // Helper
    const char *_signopt = match[1].length() == 0 ? nullptr : match[1].first;
    const char *signopt_ = match[1].length() == 0 ? nullptr : match[1].second;
    const char *_width = match[2].length() == 0 ? nullptr : match[2].first;
    const char *_precision = match[4].length() == 0 ? nullptr : match[4].first;
    const char _conversion = match[6].length() == 0 ? '\0' : match[6].first[0];

    if (_conversion == '\0') {
      goto error;
    }

    checkSign(_signopt, signopt_, f);

    charAsInt = true;

    switch (_conversion) {
      case '%':
        consume = -1;
        fmtlen--;

        os->put('%');

        break;
      case 's':
        break;
      case 'c':
        intAsChar = true;
        charAsInt = false;
        break;
      case 'd':
      case 'i':
      case 'u':
        // Decimal
        changeFlag(f | std::ios::dec);
        changeWidth(_width);

        break;
      case 'o':
        // Octal
        changeFlag(f | std::ios::oct);
        changeWidth(_width);

        break;
      case 'p':
        f |= std::ios::showbase;

        [[fallthrough]];
      case 'X':
        f |= std::ios::uppercase;

        [[fallthrough]];
      case 'x':
        // Hexadecimal
        changeFlag(f | std::ios::hex);
        changeWidth(_width);

        break;
      case 'F':
      case 'f':
        // Floating-point number in fixed format
        changeFlag(f | std::ios::fixed);
        changeWidth(_width);
        changePrecision(_precision);

        break;
      case 'E':
      case 'G':  // Not supported, just failback to scientific format
        f |= std::ios::uppercase;

        [[fallthrough]];
      case 'e':
      case 'g':  // Not supported, just failback to scientific format
        // Floating-point number in scientific format
        changeFlag(f | std::ios::scientific);
        changeWidth(_width);
        changePrecision(_precision);

        break;
      case 'A':
        f |= std::ios::uppercase;

        [[fallthrough]];
      case 'a':
        // Floating-point number in hex format
        changeFlag(f | std::ios::floatfield);
        changeWidth(_width);
        changePrecision(_precision);

        break;
      case 'n':
        goto error;
      default:
        // Not possible
        break;
    }
  }
  else {
    goto error;
  }

  return fmtlen;

error:
  err = true;

  return fmtlen;
}

}  // namespace SimpleSSD
