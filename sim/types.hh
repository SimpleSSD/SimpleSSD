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

#define INVALID_NUMBER64 std::numeric_limits<uint64_t>::max()
#define INVALID_NUMBER32 std::numeric_limits<uint32_t>::max()

static void assertNumber(uint64_t v) noexcept {
  if (UNLIKELY(v == INVALID_NUMBER64)) {
#ifdef SIMPLESSD_DEBUG
    std::cerr << "TypeError: Operation performed on invalid number."
              << std::endl;
#endif
    abort();
  }
}

static void assertNumber(uint32_t v) noexcept {
  if (UNLIKELY(v == INVALID_NUMBER32)) {
#ifdef SIMPLESSD_DEBUG
    std::cerr << "TypeError: Operation performed on invalid number."
              << std::endl;
#endif
    abort();
  }
}

#define DEFINE_NUMBER64(classname)                                             \
  class classname {                                                            \
   private:                                                                    \
    uint64_t value;                                                            \
                                                                               \
   public:                                                                     \
    classname() : value(INVALID_NUMBER64) {}                                   \
    classname(const classname &rhs) : value(rhs.value) {}                      \
    classname(classname &&rhs) noexcept { *this = std::move(rhs); }            \
    classname(uint64_t rhs) : value(rhs) {}                                    \
    classname &operator=(const classname &rhs) {                               \
      value = rhs.value;                                                       \
      return *this;                                                            \
    }                                                                          \
    classname &operator=(classname &&rhs) {                                    \
      if (this != &rhs) {                                                      \
        value = std::exchange(rhs.value, INVALID_NUMBER64);                    \
      }                                                                        \
      return *this;                                                            \
    }                                                                          \
    classname &operator=(uint64_t rhs) {                                       \
      value = rhs;                                                             \
      return *this;                                                            \
    }                                                                          \
    classname &operator+=(const classname &rhs) {                              \
      assertNumber(value);                                                     \
      assertNumber(rhs.value);                                                 \
      value += rhs.value;                                                      \
      return *this;                                                            \
    }                                                                          \
    classname &operator-=(const classname &rhs) {                              \
      assertNumber(value);                                                     \
      assertNumber(rhs.value);                                                 \
      value -= rhs.value;                                                      \
      return *this;                                                            \
    }                                                                          \
    friend classname &operator+(classname lhs, const classname &rhs) {         \
      return lhs += rhs;                                                       \
    }                                                                          \
    friend classname &operator-(classname lhs, const classname &rhs) {         \
      return lhs -= rhs;                                                       \
    }                                                                          \
    friend bool operator<(const classname &lhs, const classname &rhs) {        \
      return lhs.value < rhs.value;                                            \
    }                                                                          \
    friend bool operator>(const classname &lhs, const classname &rhs) {        \
      return lhs.value > rhs.value;                                            \
    }                                                                          \
    friend bool operator<=(const classname &lhs, const classname &rhs) {       \
      return lhs.value <= rhs.value;                                           \
    }                                                                          \
    friend bool operator>=(const classname &lhs, const classname &rhs) {       \
      return lhs.value >= rhs.value;                                           \
    }                                                                          \
    friend bool operator==(const classname &lhs, const classname &rhs) {       \
      return lhs.value == rhs.value;                                           \
    }                                                                          \
    friend bool operator!=(const classname &lhs, const classname &rhs) {       \
      return lhs.value != rhs.value;                                           \
    }                                                                          \
    explicit operator bool() const { return value != INVALID_NUMBER64; }       \
    explicit operator uint64_t() const { return value; }                       \
  }

#define DEFINE_NUMBER32(classname)                                             \
  class classname {                                                            \
   private:                                                                    \
    uint32_t value;                                                            \
                                                                               \
   public:                                                                     \
    classname() : value(INVALID_NUMBER32) {}                                   \
    classname(const classname &rhs) : value(rhs.value) {}                      \
    classname(classname &&rhs) noexcept { *this = std::move(rhs); }            \
    classname(uint64_t rhs) : value(rhs) {}                                    \
    classname &operator=(const classname &rhs) {                               \
      value = rhs.value;                                                       \
      return *this;                                                            \
    }                                                                          \
    classname &operator=(classname &&rhs) {                                    \
      if (this != &rhs) {                                                      \
        value = std::exchange(rhs.value, INVALID_NUMBER32);                    \
      }                                                                        \
      return *this;                                                            \
    }                                                                          \
    classname &operator=(uint32_t rhs) {                                       \
      value = rhs;                                                             \
      return *this;                                                            \
    }                                                                          \
    classname &operator+=(const classname &rhs) {                              \
      assertNumber(value);                                                     \
      assertNumber(rhs.value);                                                 \
      value += rhs.value;                                                      \
      return *this;                                                            \
    }                                                                          \
    classname &operator-=(const classname &rhs) {                              \
      assertNumber(value);                                                     \
      assertNumber(rhs.value);                                                 \
      value -= rhs.value;                                                      \
      return *this;                                                            \
    }                                                                          \
    friend classname &operator+(classname lhs, const classname &rhs) {         \
      return lhs += rhs;                                                       \
    }                                                                          \
    friend classname &operator-(classname lhs, const classname &rhs) {         \
      return lhs -= rhs;                                                       \
    }                                                                          \
    friend bool operator<(const classname &lhs, const classname &rhs) {        \
      return lhs.value < rhs.value;                                            \
    }                                                                          \
    friend bool operator>(const classname &lhs, const classname &rhs) {        \
      return lhs.value > rhs.value;                                            \
    }                                                                          \
    friend bool operator<=(const classname &lhs, const classname &rhs) {       \
      return lhs.value <= rhs.value;                                           \
    }                                                                          \
    friend bool operator>=(const classname &lhs, const classname &rhs) {       \
      return lhs.value >= rhs.value;                                           \
    }                                                                          \
    friend bool operator==(const classname &lhs, const classname &rhs) {       \
      return lhs.value == rhs.value;                                           \
    }                                                                          \
    friend bool operator!=(const classname &lhs, const classname &rhs) {       \
      return lhs.value != rhs.value;                                           \
    }                                                                          \
    explicit operator bool() const { return value != INVALID_NUMBER32; }       \
    explicit operator uint32_t() const { return value; }                       \
  }

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

//! Template specialization of std::hash for LPN
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
