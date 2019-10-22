// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_UTIL_BITSET_HH__
#define __SIMPLESSD_UTIL_BITSET_HH__

#include <cinttypes>
#include <iostream>
#include <vector>

namespace SimpleSSD {

/**
 * \brief Bitset object
 *
 * Same as std::vector<bool>, but size cannot be changed.
 */
class Bitset {
 private:
  // Size optimization
  union {
    uint8_t *pointer;
    uint8_t buffer[sizeof(uint8_t *)];
  };

  uint32_t dataSize;
  uint32_t allocSize;

  inline uint8_t *getBuffer() const {
    if (allocSize > sizeof(buffer)) {
      return pointer;
    }

    return (uint8_t *)buffer;
  }

 public:
  Bitset();
  Bitset(uint32_t);
  Bitset(const Bitset &) = delete;
  Bitset(Bitset &&) noexcept;
  ~Bitset();

  bool test(uint32_t) noexcept;
  bool test(uint32_t) const noexcept;
  bool all() noexcept;
  bool all() const noexcept;
  bool any() noexcept;
  bool any() const noexcept;
  bool none() noexcept;
  bool none() const noexcept;

  uint32_t clz() noexcept;  // Leading zero
  uint32_t clz() const noexcept;
  uint32_t ctz() noexcept;  // Trailing zero
  uint32_t ctz() const noexcept;

  uint32_t count() noexcept;
  uint32_t count() const noexcept;
  uint32_t size() noexcept;
  uint32_t size() const noexcept;

  void set() noexcept;
  void set(uint32_t, bool = true) noexcept;
  void reset() noexcept;
  void reset(uint32_t) noexcept;
  void flip() noexcept;
  void flip(uint32_t) noexcept;

  bool operator[](uint32_t) noexcept;
  bool operator[](uint32_t) const noexcept;
  Bitset &operator&=(const Bitset &);
  Bitset &operator|=(const Bitset &);
  Bitset &operator^=(const Bitset &);
  Bitset &operator=(const Bitset &) = delete;
  Bitset &operator=(Bitset &&);

  friend Bitset &operator&(Bitset lhs, const Bitset &rhs) { return lhs &= rhs; }
  friend Bitset &operator|(Bitset lhs, const Bitset &rhs) { return lhs |= rhs; }
  friend Bitset &operator^(Bitset lhs, const Bitset &rhs) { return lhs ^= rhs; }
  friend bool operator==(const Bitset &lhs, const Bitset &rhs) {
    bool ret = true;

    if (lhs.dataSize != rhs.dataSize) {
      std::cerr << "Size does not match" << std::endl;

      abort();
    }

    auto data = lhs.getBuffer();
    auto rdata = rhs.getBuffer();

    for (uint32_t i = 0; i < lhs.allocSize; i++) {
      if (data[i] != rdata[i]) {
        ret = false;

        break;
      }
    }

    return ret;
  }

  friend bool operator!=(const Bitset &lhs, const Bitset &rhs) {
    return !operator==(lhs, rhs);
  }

  void createCheckpoint(std::ostream &) const noexcept;
  void restoreCheckpoint(std::istream &) noexcept;
};

}  // namespace SimpleSSD

#endif
