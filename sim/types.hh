// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_TYPES_HH__
#define __SIMPLESSD_TYPES_HH__

#include <cinttypes>
#include <functional>
#include <iostream>
#include <limits>
#include <utility>

#include "util/algorithm.hh"

namespace SimpleSSD {

namespace FTL {

struct Parameter;

}

#define DEFINE_NUMBER(bits, classname)                                         \
  class classname {                                                            \
   private:                                                                    \
    uint##bits##_t value;                                                      \
    static void assertNumber(uint##bits##_t v) noexcept {                      \
      if (UNLIKELY(v == std::numeric_limits<uint##bits##_t>::max())) {         \
        std::cerr << "TypeError: Operation performed on invalid number."       \
                  << std::endl;                                                \
        abort();                                                               \
      }                                                                        \
    }                                                                          \
                                                                               \
   public:                                                                     \
    classname() : value(std::numeric_limits<uint##bits##_t>::max()) {}         \
    classname(const classname &rhs) : value(rhs.value) {}                      \
    classname(classname &&rhs) noexcept { *this = std::move(rhs); }            \
    classname(uint##bits##_t rhs) : value(rhs) {}                              \
    classname &operator=(const classname &rhs) {                               \
      value = rhs.value;                                                       \
      return *this;                                                            \
    }                                                                          \
    classname &operator=(classname &&rhs) {                                    \
      if (this != &rhs) {                                                      \
        value = std::exchange(rhs.value,                                       \
                              std::numeric_limits<uint##bits##_t>::max());     \
      }                                                                        \
      return *this;                                                            \
    }                                                                          \
    classname &operator=(uint##bits##_t rhs) {                                 \
      value = rhs;                                                             \
      return *this;                                                            \
    }                                                                          \
    classname &operator+=(const classname &rhs) {                              \
      assertNumber(value);                                                     \
      assertNumber(rhs.value);                                                 \
      value += rhs.value;                                                      \
      return *this;                                                            \
    }                                                                          \
    classname &operator+=(const uint##bits##_t &rhs) {                         \
      assertNumber(value);                                                     \
      value += rhs;                                                            \
      return *this;                                                            \
    }                                                                          \
    classname &operator-=(const classname &rhs) {                              \
      assertNumber(value);                                                     \
      assertNumber(rhs.value);                                                 \
      value -= rhs.value;                                                      \
      return *this;                                                            \
    }                                                                          \
    classname &operator-=(const uint##bits##_t &rhs) {                         \
      assertNumber(value);                                                     \
      value -= rhs;                                                            \
      return *this;                                                            \
    }                                                                          \
    classname &operator*=(const uint##bits##_t &rhs) {                         \
      assertNumber(value);                                                     \
      value *= rhs;                                                            \
      return *this;                                                            \
    }                                                                          \
    classname &operator/=(const uint##bits##_t &rhs) {                         \
      assertNumber(value);                                                     \
      value /= rhs;                                                            \
      return *this;                                                            \
    }                                                                          \
    classname &operator%=(const uint##bits##_t &rhs) {                         \
      assertNumber(value);                                                     \
      value %= rhs;                                                            \
      return *this;                                                            \
    }                                                                          \
    classname &operator&=(const uint##bits##_t &rhs) {                         \
      assertNumber(value);                                                     \
      value &= rhs;                                                            \
      return *this;                                                            \
    }                                                                          \
    classname &operator|=(const uint##bits##_t &rhs) {                         \
      assertNumber(value);                                                     \
      value |= rhs;                                                            \
      return *this;                                                            \
    }                                                                          \
    inline operator uint##bits##_t() const { return value; }                   \
    inline bool isValid() const {                                              \
      return value != std::numeric_limits<uint##bits##_t>::max();              \
    }                                                                          \
    inline void invalidate() {                                                 \
      value = std::numeric_limits<uint##bits##_t>::max();                      \
    }                                                                          \
  }

#define DEFINE_NUMBER32(classname) DEFINE_NUMBER(32, classname)
#define DEFINE_NUMBER64(classname) DEFINE_NUMBER(64, classname)

//! Logical Page Number
DEFINE_NUMBER64(LPN);

//! Logical Superpage Number
DEFINE_NUMBER64(LSPN);

//! Physical Page Number
DEFINE_NUMBER64(PPN);

//! Physical Superpage Number
DEFINE_NUMBER64(PSPN);

//! Physical Block Number
DEFINE_NUMBER32(PBN);

//! Physical Superblock Number
DEFINE_NUMBER32(PSBN);

}  // namespace SimpleSSD

//! Template specialization of std::hash
namespace std {

template <>
struct hash<SimpleSSD::LPN> {
  std::size_t operator()(SimpleSSD::LPN const &s) const noexcept {
    return std::hash<uint64_t>{}(static_cast<uint64_t>(s));
  }
};

template <>
struct hash<SimpleSSD::PPN> {
  std::size_t operator()(SimpleSSD::PPN const &s) const noexcept {
    return std::hash<uint64_t>{}(static_cast<uint64_t>(s));
  }
};

}  // namespace std

#endif
