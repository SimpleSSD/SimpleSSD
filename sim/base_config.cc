// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "sim/base_config.hh"

#include <cstdarg>
#include <iostream>
#include <regex>
#include <vector>

namespace SimpleSSD {

const std::regex regexInteger("(\\d+)([kKmMgGtTpP]?)",
                              std::regex_constants::ECMAScript);
const std::regex regexTime("(\\d+)([munp]?s?)",
                           std::regex_constants::ECMAScript |
                               std::regex_constants::icase);

//! A constructor
BaseConfig::BaseConfig() {}

int64_t BaseConfig::convertInt(const char *value, bool *valid) noexcept {
  int64_t ret = 0;
  bool negative = false;
  std::cmatch match;

  if (valid) {
    *valid = false;
  }

  if (value[0] == '-') {
    value++;
    negative = true;
  }

  if (std::regex_match(value, match, regexInteger)) {
    if (valid) {
      *valid = true;
    }

    ret = (int64_t)strtoll(match[1].str().c_str(), nullptr, 10);

    if (match[2].length() > 0) {
      switch (match[2].str().at(0)) {
        case 'k':
          ret *= 1000ll;
          break;
        case 'K':
          ret *= 1024ll;
          break;
        case 'm':
          ret *= 1000000ll;
          break;
        case 'M':
          ret *= 1048576ll;
          break;
        case 'g':
          ret *= 1000000000ll;
          break;
        case 'G':
          ret *= 1073741824ll;
          break;
        case 't':
          ret *= 1000000000000ll;
          break;
        case 'T':
          ret *= 1099511627776ll;
          break;
      }
    }
  }

  if (negative) {
    return -ret;
  }

  return ret;
}

uint64_t BaseConfig::convertUint(const char *value, bool *valid) noexcept {
  uint64_t ret = 0;
  std::cmatch match;

  if (valid) {
    *valid = false;
  }

  if (std::regex_match(value, match, regexInteger)) {
    if (valid) {
      *valid = true;
    }

    ret = (uint64_t)strtoull(match[1].str().c_str(), nullptr, 10);

    if (match[2].length() > 0) {
      switch (match[2].str().at(0)) {
        case 'k':
          ret *= 1000ull;
          break;
        case 'K':
          ret *= 1024ull;
          break;
        case 'm':
          ret *= 1000000ull;
          break;
        case 'M':
          ret *= 1048576ull;
          break;
        case 'g':
          ret *= 1000000000ull;
          break;
        case 'G':
          ret *= 1073741824ull;
          break;
        case 't':
          ret *= 1000000000000ull;
          break;
        case 'T':
          ret *= 1099511627776ull;
          break;
      }
    }
  }

  return ret;
}

uint64_t BaseConfig::convertTime(const char *value, bool *valid) noexcept {
  uint64_t ret = 0;
  std::cmatch match;

  if (valid) {
    *valid = false;
  }

  if (std::regex_match(value, match, regexTime)) {
    if (valid) {
      *valid = true;
    }

    ret = (uint64_t)strtoull(match[1].str().c_str(), nullptr, 10);

    if (match[2].length() > 0) {
      switch (match[2].str().at(0)) {
        case 's':
          ret *= 1000000000000ul;
          break;
        case 'm':
          ret *= 1000000000ul;
          break;
        case 'u':
          ret *= 1000000ul;
          break;
        case 'n':
          ret *= 1000ul;
          break;
        case 'p':
          // Do Nothing
          break;
      }
    }
  }

  return ret;
}

std::string BaseConfig::formatInt(int64_t value) {
  bool negative = value < 0;
  std::string ret;

  if (negative) {
    ret += '-';
  }

  ret += formatUint(negative ? -value : value);

  return ret;
}

std::string BaseConfig::formatUint(uint64_t value) {
  char suffix = '\0';

  if (value % 1099511627776ull == 0) {
    value /= 1099511627776ull;
    suffix = 'T';
  }
  else if (value % 1000000000000ull == 0) {
    value /= 1000000000000ull;
    suffix = 't';
  }
  else if (value % 1073741824ull == 0) {
    value /= 1073741824ull;
    suffix = 'G';
  }
  else if (value % 1000000000ull == 0) {
    value /= 1000000000ull;
    suffix = 'g';
  }
  else if (value % 1048576ull == 0) {
    value /= 1048576ull;
    suffix = 'M';
  }
  else if (value % 1000000ull == 0) {
    value /= 1000000ull;
    suffix = 'm';
  }
  else if (value % 1024ull == 0) {
    value /= 1024ull;
    suffix = 'K';
  }
  else if (value % 1000ull == 0) {
    value /= 1000ull;
    suffix = 'k';
  }

  return std::to_string(value) + suffix;
}

std::string BaseConfig::formatTime(uint64_t value) {
  char *suffix = "\0";

  if (value % 1000000000000ull == 0) {
    value /= 1000000000000ull;
    suffix = "s";
  }
  else if (value % 1000000000ull == 0) {
    value /= 1000000000ull;
    suffix = "ms";
  }
  else if (value % 1000000ull == 0) {
    value /= 1000000ull;
    suffix = "us";
  }
  else if (value % 1000ull == 0) {
    value /= 1000ull;
    suffix = "ns";
  }

  return std::to_string(value) + suffix;
}

bool BaseConfig::isSection(pugi::xml_node &node) noexcept {
  return strcmp(node.name(), CONFIG_SECTION_NAME) == 0;
}

bool BaseConfig::isKey(pugi::xml_node &node) noexcept {
  return strcmp(node.name(), CONFIG_KEY_NAME) == 0;
}

void BaseConfig::panic_if(bool eval, const char *format, ...) noexcept {
#ifndef EXCLUDE_CPU
  if (eval) {
    va_list copy, args;
    std::vector<char> str;

    va_start(args, format);
    va_copy(copy, args);
    str.resize(vsnprintf(nullptr, 0, format, copy) + 1);
    va_end(copy);
    vsnprintf(str.data(), str.size(), format, args);
    va_end(args);

    std::cerr << "panic: " << str.data() << std::endl;
    abort();
  }
#endif
}

void BaseConfig::warn_if(bool eval, const char *format, ...) noexcept {
#ifndef EXCLUDE_CPU
  if (eval) {
    va_list copy, args;
    std::vector<char> str;

    va_start(args, format);
    va_copy(copy, args);
    str.resize(vsnprintf(nullptr, 0, format, copy) + 1);
    va_end(copy);
    vsnprintf(str.data(), str.size(), format, args);
    va_end(args);

    std::cerr << "warn: " << str.data() << std::endl;
  }
#endif
}

}  // namespace SimpleSSD
