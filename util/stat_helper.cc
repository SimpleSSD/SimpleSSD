// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "util/stat_helper.hh"

namespace SimpleSSD {

IOStat::IOStat() : count(0), size(0) {}

void IOStat::add(uint64_t reqsize) noexcept {
  count++;
  size += reqsize;
}

uint64_t IOStat::getCount() noexcept {
  return count;
}

uint64_t IOStat::getSize() noexcept {
  return size;
}

void IOStat::clear() noexcept {
  count = 0;
  size = 0;
}

void IOStat::createCheckpoint(std::ostream &out) const noexcept {
  BACKUP_SCALAR(out, count);
  BACKUP_SCALAR(out, size);
}

void IOStat::restoreCheckpoint(std::istream &in) noexcept {
  RESTORE_SCALAR(in, count);
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

uint64_t BusyStat::getBusyTick() noexcept {
  return totalBusy;
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

}  // namespace SimpleSSD
