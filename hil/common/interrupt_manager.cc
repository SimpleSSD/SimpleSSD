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
    : pending(false), currentRequestCount(0), nextDeadline(0) {}

InterruptManager::InterruptManager(ObjectData &o, Interface *i, ControllerID id)
    : Object(o),
      pInterface(i),
      controllerID(id),
      aggregationThreshold(0),
      aggregationTime(0),
      coalesceMap([](const CoalesceData *a, const CoalesceData *b) -> bool {
        return a->nextDeadline < b->nextDeadline;
      }) {
  eventTimer = createEvent([this](uint64_t t, uint64_t) { timerHandler(t); },
                           "HIL::InterruptManager::eventTimer");
}

InterruptManager::~InterruptManager() {}

void InterruptManager::timerHandler(uint64_t tick) {
  auto data = coalesceMap.begin();

  panic_if(data->second->nextDeadline != tick,
           "Timer broken in interrupt coalescing.");

  data->second->currentRequestCount = 0;
  data->second->nextDeadline = std::numeric_limits<uint64_t>::max();
  data->second->pending = true;

  pInterface->postInterrupt(data->first, true);

  // Schedule Timer
  reschedule(data);
}

void InterruptManager::reschedule(
    map_map<uint16_t, CoalesceData *>::iterator iter) {
  if (iter != coalesceMap.end()) {
    auto iv = iter->first;
    auto data = iter->second;

    // Sort list again
    coalesceMap.erase(iter);
    coalesceMap.insert(iv, data);
  }

  if (LIKELY(coalesceMap.size() > 0)) {
    auto next = coalesceMap.front();

    scheduleAbs(eventTimer, 0ull, next.second->nextDeadline);
  }
}

void InterruptManager::postInterrupt(uint16_t iv, bool set) {
  bool immediate = false;
  auto iter = coalesceMap.find(iv);

  if (iter != coalesceMap.end()) {
    if (set) {
      iter->second->currentRequestCount++;

      if (iter->second->currentRequestCount == 1) {
        iter->second->nextDeadline = getTick() + aggregationTime;

        // Schedule Timer
        reschedule(iter);
      }
      else if (iter->second->currentRequestCount >= aggregationThreshold) {
        immediate = true;

        iter->second->currentRequestCount = 0;
        iter->second->nextDeadline = std::numeric_limits<uint64_t>::max();
        iter->second->pending = true;

        // Schedule Timer
        reschedule(iter);
      }
    }
    else if (iter->second->pending) {
      immediate = true;

      iter->second->pending = false;
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

  debugprint_ctrl("INTR    | %s interrupt coalescing | IV %u",
                  set ? "Enable" : "Disable", iv);

  if (set && iter == coalesceMap.end()) {
    CoalesceData *tmp = new CoalesceData();

    tmp->currentRequestCount = 0;
    tmp->nextDeadline = 0;

    coalesceMap.insert(iv, tmp);
  }
  else if (!set && iter != coalesceMap.end()) {
    delete iter->second;

    coalesceMap.erase(iter);

    reschedule(coalesceMap.end());
  }
}

bool InterruptManager::isEnabled(uint16_t iv) {
  auto iter = coalesceMap.find(iv);

  return iter != coalesceMap.end();
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

void InterruptManager::createCheckpoint(std::ostream &out) const noexcept {
  BACKUP_SCALAR(out, aggregationThreshold);
  BACKUP_SCALAR(out, aggregationTime);

  BACKUP_EVENT(out, eventTimer);

  // Store list by poping all elements
  // Larger one first - for faster restore
  auto size = coalesceMap.size();
  BACKUP_SCALAR(out, size);

  for (auto &iter : coalesceMap) {
    BACKUP_SCALAR(out, iter.first);
    BACKUP_SCALAR(out, iter.second->pending);
    BACKUP_SCALAR(out, iter.second->currentRequestCount);
    BACKUP_SCALAR(out, iter.second->nextDeadline);
  }
}

void InterruptManager::restoreCheckpoint(std::istream &in) noexcept {
  RESTORE_SCALAR(in, aggregationThreshold);
  RESTORE_SCALAR(in, aggregationTime);

  RESTORE_EVENT(in, eventTimer);

  // Load list by pushping all elements
  uint64_t size;
  RESTORE_SCALAR(in, size);

  for (uint64_t i = 0; i < size; i++) {
    uint16_t iv;
    auto iter = new CoalesceData();

    RESTORE_SCALAR(in, iv);
    RESTORE_SCALAR(in, iter->pending);
    RESTORE_SCALAR(in, iter->currentRequestCount);
    RESTORE_SCALAR(in, iter->nextDeadline);

    coalesceMap.insert(iv, iter);
  }
}

}  // namespace SimpleSSD::HIL
