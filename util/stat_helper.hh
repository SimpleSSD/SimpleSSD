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

class CountStat {
 private:
  uint64_t count;

 public:
  CountStat();

  void add() noexcept;
  void add(uint64_t) noexcept;

  uint64_t getCount() noexcept;

  virtual void clear() noexcept;

  virtual void createCheckpoint(std::ostream &) const noexcept;
  virtual void restoreCheckpoint(std::istream &) noexcept;
};

class RatioStat : public CountStat {
 private:
  uint64_t hit;

 public:
  RatioStat();

  void add() noexcept = delete;
  void add(uint64_t) noexcept = delete;

  void addHit() noexcept;
  void addHit(uint64_t) noexcept;
  void addMiss() noexcept;
  void addMiss(uint64_t) noexcept;

  uint64_t getHitCount() noexcept;
  uint64_t getTotalCount() noexcept;
  double getRatio() noexcept;

  void clear() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

class SizeStat : public CountStat {
 private:
  uint64_t size;

 public:
  SizeStat();

  void add() noexcept = delete;
  void add(uint64_t) noexcept;

  uint64_t getSize() noexcept;

  void clear() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
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

class LatencyStat : public SizeStat {
 private:
  uint64_t total;
  uint64_t min;
  uint64_t max;

 public:
  LatencyStat();

  void add() noexcept = delete;
  void add(uint64_t) noexcept = delete;
  void add(uint64_t, uint64_t) noexcept;

  uint64_t getAverageLatency() noexcept;
  uint64_t getMinimumLatency() noexcept;
  uint64_t getMaximumLatency() noexcept;
  uint64_t getTotalLatency() noexcept;

  void clear() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD

#endif
