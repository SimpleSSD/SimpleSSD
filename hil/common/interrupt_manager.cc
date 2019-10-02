// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "hil/common/interrupt_manager.hh"

namespace SimpleSSD::HIL {

InterruptManager::CoalesceData::CoalesceData()
    : iv(0xFFFF), pending(false), currentRequestCount(0), nextDeadline(0) {}

InterruptManager::InterruptManager(ObjectData &&o, Interface *i)
    : Object(std::move(o)),
      pInterface(i),
      interruptCoalescing(false),
      aggregationTime(0),
      aggregationThreshold(0) {}

InterruptManager::~InterruptManager() {}

void InterruptManager::timerHandler(uint64_t tick, CoalesceData *data) {
  panic_if(data->nextDeadline != tick, "Timer broken in interrupt coalescing.");

  data->currentRequestCount = 0;
  data->nextDeadline = 0;
  data->pending = true;

  pInterface->postInterrupt(data->iv, true);
}

void InterruptManager::postInterrupt(uint16_t iv, bool set) {
  bool immediate = false;
  auto iter = coalesceMap.find(iv);

  if (iter != coalesceMap.end()) {
    if (set) {
      iter->second.currentRequestCount++;

      if (iter->second.currentRequestCount == 1) {
        iter->second.nextDeadline = getTick() + aggregationTime;
        schedule(iter->second.timerEvent, iter->second.nextDeadline,
                 &iter->second);
      }
      else if (iter->second.currentRequestCount >= aggregationThreshold) {
        immediate = true;

        iter->second.currentRequestCount = 0;
        iter->second.nextDeadline = 0;
        iter->second.pending = true;

        deschedule(iter->second.timerEvent);
      }
    }
    else if (iter->second.pending) {
      immediate = true;

      iter->second.pending = false;
    }
  }
  else {
    immediate = true;
  }

  if (immediate) {
    pInterface->postInterrupt(iv, set);
  }
}

void InterruptManager::enableCoalescing(bool set, uint16_t iv) {
  panic_if(aggregationTime == 0 || aggregationThreshold == 0,
           "Interrupt coalesceing parameters are not set.");
  panic_if(iv == 0xFFFF, "Invalid interrupt vector.");

  auto iter = coalesceMap.find(iv);

  if (set && iter == coalesceMap.end()) {
    CoalesceData tmp;

    tmp.timerEvent = createEvent(
        [this](uint64_t t, void *c) { timerHandler(t, (CoalesceData *)c); },
        "HIL::InterruptManager::CoalesceData(iv:" + std::to_string(iv) +
            ")::timerEvent");
    tmp.iv = iv;
    tmp.currentRequestCount = 0;
    tmp.nextDeadline = 0;

    coalesceMap.emplace(std::make_pair(iv, tmp));
  }
  else if (!set && iter != coalesceMap.end()) {
    destroyEvent(iter->second.timerEvent);

    coalesceMap.erase(iter);
  }
}  // namespace SimpleSSD::HIL

void InterruptManager::configureCoalescing(uint64_t time, uint16_t count) {
  panic_if(time == 0 || count < 2, "Invalid coalescing parameters.");

  aggregationTime = time;
  aggregationThreshold = count;
}

}  // namespace SimpleSSD::HIL
