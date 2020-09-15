// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "util/stat_helper.hh"

namespace SimpleSSD {

CountStat::CountStat() : count(0) {}

void CountStat::add() noexcept {
  count++;
}

void CountStat::add(uint64_t v) noexcept {
  count += v;
}

uint64_t CountStat::getCount() noexcept {
  return count;
}

void CountStat::clear() noexcept {
  count = 0;
}

void CountStat::createCheckpoint(std::ostream &out) const noexcept {
  BACKUP_SCALAR(out, count);
}

void CountStat::restoreCheckpoint(std::istream &in) noexcept {
  RESTORE_SCALAR(in, count);
}

RatioStat::RatioStat() : CountStat(), hit(0) {}

void RatioStat::addHit() noexcept {
  CountStat::add();

  hit++;
}

void RatioStat::addHit(uint64_t v) noexcept {
  CountStat::add(v);

  hit += v;
}

void RatioStat::addMiss() noexcept {
  CountStat::add();
}

void RatioStat::addMiss(uint64_t v) noexcept {
  CountStat::add(v);
}

uint64_t RatioStat::getHitCount() noexcept {
  return hit;
}

uint64_t RatioStat::getTotalCount() noexcept {
  return CountStat::getCount();
}

double RatioStat::getRatio() noexcept {
  auto total = CountStat::getCount();

  if (total > 0) {
    return (double)hit / CountStat::getCount();
  }
  else {
    return 0.;
  }
}

void RatioStat::clear() noexcept {
  CountStat::clear();

  hit = 0;
}

void RatioStat::createCheckpoint(std::ostream &out) const noexcept {
  CountStat::createCheckpoint(out);

  BACKUP_SCALAR(out, hit);
}

void RatioStat::restoreCheckpoint(std::istream &in) noexcept {
  CountStat::restoreCheckpoint(in);

  RESTORE_SCALAR(in, hit);
}

SizeStat::SizeStat() : CountStat(), size(0) {}

void SizeStat::add(uint64_t reqsize) noexcept {
  CountStat::add();

  size += reqsize;
}

uint64_t SizeStat::getSize() noexcept {
  return size;
}

void SizeStat::clear() noexcept {
  CountStat::clear();

  size = 0;
}

void SizeStat::createCheckpoint(std::ostream &out) const noexcept {
  CountStat::createCheckpoint(out);

  BACKUP_SCALAR(out, size);
}

void SizeStat::restoreCheckpoint(std::istream &in) noexcept {
  CountStat::restoreCheckpoint(in);

  RESTORE_SCALAR(in, size);
}

BusyStat::BusyStat() : isBusy(false), depth(0), lastBusyAt(0), totalBusy(0) {}

void BusyStat::busyBegin(uint64_t now) noexcept {
  if (!isBusy) {
    isBusy = true;
    lastBusyAt = now;
  }

  depth++;
}

void BusyStat::busyEnd(uint64_t now) noexcept {
  if (isBusy) {
    depth--;

    if (depth == 0) {
      isBusy = false;

      totalBusy += now - lastBusyAt;
    }
  }
}

uint64_t BusyStat::getBusyTick(uint64_t now) noexcept {
  if (isBusy) {
    return totalBusy + (now - lastBusyAt);
  }
  else {
    return totalBusy;
  }
}

void BusyStat::clear(uint64_t now) noexcept {
  if (isBusy) {
    lastBusyAt = now;
  }

  totalBusy = 0;
}

void BusyStat::createCheckpoint(std::ostream &out) const noexcept {
  BACKUP_SCALAR(out, isBusy);
  BACKUP_SCALAR(out, depth);
  BACKUP_SCALAR(out, lastBusyAt);
  BACKUP_SCALAR(out, totalBusy);
}

void BusyStat::restoreCheckpoint(std::istream &in) noexcept {
  RESTORE_SCALAR(in, isBusy);
  RESTORE_SCALAR(in, depth);
  RESTORE_SCALAR(in, lastBusyAt);
  RESTORE_SCALAR(in, totalBusy);
}

LatencyStat::LatencyStat()
    : SizeStat(), total(0), min(std::numeric_limits<uint64_t>::max()), max(0) {}

void LatencyStat::add(uint64_t size, uint64_t latency) noexcept {
  SizeStat::add(size);

  total += latency;
  min = MIN(min, latency);
  max = MAX(max, latency);
}

uint64_t LatencyStat::getAverageLatency() noexcept {
  auto count = SizeStat::getCount();

  if (count > 0) {
    return total / count;
  }

  return 0;
}

uint64_t LatencyStat::getMinimumLatency() noexcept {
  auto count = SizeStat::getCount();

  if (count > 0) {
    return min;
  }

  return 0;
}

uint64_t LatencyStat::getMaximumLatency() noexcept {
  return max;
}

uint64_t LatencyStat::getTotalLatency() noexcept {
  return total;
}

void LatencyStat::clear() noexcept {
  SizeStat::clear();

  total = 0;
  min = std::numeric_limits<uint64_t>::max();
  max = 0;
}

void LatencyStat::createCheckpoint(std::ostream &out) const noexcept {
  SizeStat::createCheckpoint(out);

  BACKUP_SCALAR(out, total);
  BACKUP_SCALAR(out, min);
  BACKUP_SCALAR(out, max);
}

void LatencyStat::restoreCheckpoint(std::istream &in) noexcept {
  SizeStat::restoreCheckpoint(in);

  RESTORE_SCALAR(in, total);
  RESTORE_SCALAR(in, min);
  RESTORE_SCALAR(in, max);
}

}  // namespace SimpleSSD
