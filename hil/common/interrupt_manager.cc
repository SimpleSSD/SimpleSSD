// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "hil/common/interrupt_manager.hh"

namespace SimpleSSD::HIL {

#define debugprint_ctrl(format, ...)                                           \
  {                                                                            \
    debugprint(Log::DebugID::HIL_NVMe, "CTRL %-3d | " format, controllerID,    \
               ##__VA_ARGS__);                                                 \
  }

InterruptManager::CoalesceData::CoalesceData()
    : pending(false), iv(0xFFFF), currentRequestCount(0), nextDeadline(0) {}

InterruptManager::InterruptManager(ObjectData &o, Interface *i, ControllerID id)
    : Object(o),
      pInterface(i),
      controllerID(id),
      interruptCoalescing(false),
      aggregationThreshold(0),
      aggregationTime(0),
      coalesceMap([](const void *a, const void *b) -> bool {
        return ((CoalesceData *)a)->nextDeadline <
               ((CoalesceData *)b)->nextDeadline;
      }) {
  eventTimer = createEvent([this](uint64_t t) { timerHandler(t); },
                           "HIL::InterruptManager::eventTimer");
}

InterruptManager::~InterruptManager() {}

void InterruptManager::timerHandler(uint64_t tick) {
  auto data = (CoalesceData *)coalesceMap.front();

  panic_if(data->nextDeadline != tick, "Timer broken in interrupt coalescing.");

  data->currentRequestCount = 0;
  data->nextDeadline = std::numeric_limits<uint64_t>::max();
  data->pending = true;

  pInterface->postInterrupt(data->iv, true);

  // Schedule Timer
  reschedule(data);
}

void InterruptManager::reschedule(CoalesceData *data) {
  if (data) {
    // Sort list again
    coalesceMap.erase(data->iv);
    coalesceMap.insert(data->iv, data);
  }

  if (LIKELY(coalesceMap.size() > 0)) {
    auto next = (CoalesceData *)coalesceMap.front();

    schedule(eventTimer, next->nextDeadline);
  }
}

void InterruptManager::postInterrupt(uint16_t iv, bool set) {
  bool immediate = false;
  auto iter = (CoalesceData *)coalesceMap.find(iv);

  if (iter) {
    if (set) {
      iter->currentRequestCount++;

      if (iter->currentRequestCount == 1) {
        iter->nextDeadline = getTick() + aggregationTime;

        // Schedule Timer
        reschedule(iter);
      }
      else if (iter->currentRequestCount >= aggregationThreshold) {
        immediate = true;

        iter->currentRequestCount = 0;
        iter->nextDeadline = std::numeric_limits<uint64_t>::max();
        iter->pending = true;

        // Schedule Timer
        reschedule(iter);
      }
    }
    else if (iter->pending) {
      immediate = true;

      iter->pending = false;
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

  auto iter = (CoalesceData *)coalesceMap.find(iv);

  debugprint_ctrl("INTR    | %s interrupt coalescing | IV %u",
                  set ? "Enable" : "Disable", iv);

  if (set && !iter) {
    CoalesceData *tmp = new CoalesceData();

    tmp->iv = iv;
    tmp->currentRequestCount = 0;
    tmp->nextDeadline = 0;

    coalesceMap.insert(iv, tmp);
  }
  else if (!set && iter) {
    coalesceMap.erase(iv);

    delete iter;

    reschedule(nullptr);
  }
}

bool InterruptManager::isEnabled(uint16_t iv) {
  auto iter = (CoalesceData *)coalesceMap.find(iv);

  return iter != nullptr;
}

void InterruptManager::configureCoalescing(uint64_t time, uint16_t count) {
  panic_if(time == 0 || count < 2, "Invalid coalescing parameters.");

  debugprint_ctrl("INTR    | Update coalescing parameters | TIME %u | THRES %u",
                  time / 100000000, count);

  aggregationTime = time;
  aggregationThreshold = count;
}

void InterruptManager::getCoalescing(uint64_t &time, uint16_t &count) {
  time = aggregationTime;
  count = aggregationThreshold;
}

void InterruptManager::getStatList(std::vector<Stat> &, std::string) noexcept {}

void InterruptManager::getStatValues(std::vector<double> &) noexcept {}

void InterruptManager::resetStatValues() noexcept {}

void InterruptManager::createCheckpoint(std::ostream &out) noexcept {
  BACKUP_SCALAR(out, interruptCoalescing);
  BACKUP_SCALAR(out, aggregationThreshold);
  BACKUP_SCALAR(out, aggregationTime);

  // Store list by poping all elements
  // Larger one first - for faster restore
  auto size = coalesceMap.size();
  BACKUP_SCALAR(out, size);

  for (auto iter = coalesceMap.begin(); iter != coalesceMap.end(); ++iter) {
    auto entry = (CoalesceData *)iter.getValue();

    BACKUP_SCALAR(out, entry->pending);
    BACKUP_SCALAR(out, entry->iv);
    BACKUP_SCALAR(out, entry->currentRequestCount);
    BACKUP_SCALAR(out, entry->nextDeadline);
  }
}

void InterruptManager::restoreCheckpoint(std::istream &in) noexcept {
  RESTORE_SCALAR(in, interruptCoalescing);
  RESTORE_SCALAR(in, aggregationThreshold);
  RESTORE_SCALAR(in, aggregationTime);

  // Load list by pushping all elements
  uint64_t size;
  RESTORE_SCALAR(in, size);

  for (uint64_t i = 0; i < size; i++) {
    auto iter = new CoalesceData();

    RESTORE_SCALAR(in, iter->pending);
    RESTORE_SCALAR(in, iter->iv);
    RESTORE_SCALAR(in, iter->currentRequestCount);
    RESTORE_SCALAR(in, iter->nextDeadline);

    coalesceMap.insert(iter->iv, iter);
  }
}

}  // namespace SimpleSSD::HIL
