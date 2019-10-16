// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_UTIL_SCHEDULER_HH__
#define __SIMPLESSD_UTIL_SCHEDULER_HH__

#include <deque>

#include "sim/object.hh"

namespace SimpleSSD {

template <class Type,
          std::enable_if_t<std::is_pointer_v<Type>, Type> * = nullptr>
class SingleScheduler : public Object {
 public:
  using preFunction = std::function<uint64_t(Type)>;
  using postFunction = std::function<void(Type)>;
  using backupFunction = std::function<void(std::ostream &, Type)>;
  using restoreFunction = std::function<Type(std::istream &, ObjectData &)>;

 private:
  bool pending;

  Event eventDone;

  std::deque<Type> queue;
  std::deque<Type> pendingQueue;

  void submit() {
    auto data = std::move(queue.front());

    queue.pop_front();
    pending = true;

    auto delay = preSubmit(data);
    pendingQueue.emplace_back(data);

    scheduleRel(eventDone, 0ull, delay);
  }

  void done() {
    auto data = std::move(pendingQueue.front());

    pendingQueue.pop_front();
    postDone(data);

    if (queue.size() > 0) {
      submit();
    }
    else {
      pending = false;
    }
  }

  preFunction preSubmit;
  postFunction postDone;

  backupFunction backupItem;
  restoreFunction restoreItem;

 public:
  SingleScheduler(ObjectData &o, std::string prefix, preFunction p,
                  postFunction po, backupFunction b, restoreFunction r)
      : Object(o),
        pending(false),
        eventDone(InvalidEventID),
        preSubmit(p),
        postDone(po),
        backupItem(b),
        restoreItem(r) {
    // Create events
    eventDone = createEvent([this](uint64_t, uint64_t) { done(); },
                            prefix + "::eventDone");
  }

  void enqueue(Type data) {
    queue.emplace_back(data);

    if (!pending) {
      submit();
    }
  }

  void getStatList(std::vector<Stat> &, std::string) noexcept override {}
  void getStatValues(std::vector<double> &) noexcept override {}
  void resetStatValues() noexcept override {}

  void createCheckpoint(std::ostream &out) const noexcept override {
    BACKUP_EVENT(out, eventDone);
    BACKUP_SCALAR(out, pending);

    uint64_t size = queue.size();
    BACKUP_SCALAR(out, size);

    for (auto &iter : queue) {
      backupItem(out, iter);
    }

    size = pendingQueue.size();
    BACKUP_SCALAR(out, size);

    for (auto &iter : pendingQueue) {
      backupItem(out, iter);
    }
  }

  void restoreCheckpoint(std::istream &in) noexcept override {
    RESTORE_EVENT(in, eventDone);
    RESTORE_SCALAR(in, pending);

    uint64_t size;
    RESTORE_SCALAR(in, size);

    for (uint64_t i = 0; i < size; i++) {
      auto item = restoreItem(in, object);

      queue.emplace_back(item);
    }

    RESTORE_SCALAR(in, size);

    for (uint64_t i = 0; i < size; i++) {
      auto item = restoreItem(in, object);

      pendingQueue.emplace_back(item);
    }
  }
};

template <class Type,
          std::enable_if_t<std::is_pointer_v<Type>, Type> * = nullptr>
class Scheduler : public Object {
 public:
  using preFunction = typename SingleScheduler<Type>::preFunction;
  using postFunction = typename SingleScheduler<Type>::postFunction;
  using backupFunction = typename SingleScheduler<Type>::backupFunction;
  using restoreFunction = typename SingleScheduler<Type>::restoreFunction;

 private:
  SingleScheduler<Type> readScheduler;
  SingleScheduler<Type> writeScheduler;

 public:
  Scheduler(ObjectData &o, std::string prefix, preFunction pr, preFunction pw,
            postFunction por, postFunction pow, backupFunction b,
            restoreFunction r)
      : Object(o),
        readScheduler(o, prefix + "::readScheduler", pr, por, b, r),
        writeScheduler(o, prefix + "::writeScheduler", pw, pow, b, r) {}

  void read(Type data) { readScheduler.enqueue(data); }
  void write(Type data) { writeScheduler.enqueue(data); }

  void getStatList(std::vector<Stat> &, std::string) noexcept override {}
  void getStatValues(std::vector<double> &) noexcept override {}
  void resetStatValues() noexcept override {}

  void createCheckpoint(std::ostream &out) const noexcept override {
    readScheduler.createCheckpoint(out);
    writeScheduler.createCheckpoint(out);
  }

  void restoreCheckpoint(std::istream &in) noexcept override {
    readScheduler.restoreCheckpoint(in);
    writeScheduler.restoreCheckpoint(in);
  }
};

}  // namespace SimpleSSD

#endif
