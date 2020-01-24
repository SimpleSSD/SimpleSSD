// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_UTIL_STAT_HELPER_HH__
#define __SIMPLESSD_UTIL_STAT_HELPER_HH__

#include "sim/checkpoint.hh"

namespace SimpleSSD {

class IOStat {
 private:
  uint64_t count;
  uint64_t size;

 public:
  IOStat();

  void add(uint64_t) noexcept;

  uint64_t getCount() noexcept;
  uint64_t getSize() noexcept;

  void clear() noexcept;

  void createCheckpoint(std::ostream &) const noexcept;
  void restoreCheckpoint(std::istream &) noexcept;
};

class BusyStat {
 private:
  bool isBusy;
  uint32_t depth;
  uint64_t lastBusyAt;
  uint64_t totalBusy;

 public:
  BusyStat();

  void busyBegin(uint64_t) noexcept;
  void busyEnd(uint64_t) noexcept;

  uint64_t getBusyTick(uint64_t) noexcept;

  void clear(uint64_t) noexcept;

  void createCheckpoint(std::ostream &) const noexcept;
  void restoreCheckpoint(std::istream &) noexcept;
};

}  // namespace SimpleSSD

#endif
