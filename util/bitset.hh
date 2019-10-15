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
#include <vector>

#include "sim/object.hh"

namespace SimpleSSD {

/**
 * \brief Bitset object
 *
 * Same as std::vector<bool>, but size cannot changed.
 */
class Bitset : public Object {
 private:
  uint8_t *data;
  uint32_t dataSize;
  uint32_t allocSize;

 public:
  Bitset(ObjectData &);
  Bitset(ObjectData &, uint32_t);
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
      panic("Size does not match");
    }

    for (uint32_t i = 0; i < lhs.allocSize; i++) {
      if (lhs.data[i] != rhs.data[i]) {
        ret = false;

        break;
      }
    }

    return ret;
  }

  friend bool operator!=(const Bitset &lhs, const Bitset &rhs) {
    return !operator==(lhs, rhs);
  }

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD

#endif
