// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_UTIL_SCHEDULER_HH__
#define __SIMPLESSD_UTIL_SCHEDULER_HH__

#include <list>

#include "sim/object.hh"

namespace SimpleSSD {

template <class Type,
          std::enable_if_t<std::is_pointer_v<Type>, Type> * = nullptr>
class Scheduler : public Object {
 public:
  using preFunction = std::function<uint64_t(Type)>;
  using postFunction = std::function<void(Type)>;
  using backupFunction = std::function<void(std::ostream &, Type)>;
  using restoreFunction = std::function<Type(std::istream &)>;

 private:
  bool readPending;
  bool writePending;

  Event eventReadDone;
  Event eventWriteDone;

  std::list<Type> readQueue;
  std::list<Type> readPendingQueue;
  std::list<Type> writeQueue;
  std::list<Type> writePendingQueue;

  void submitRead() {
    auto data = std::move(readQueue.front());

    readQueue.pop_front();
    readPending = true;

    auto delay = preSubmitRead(data);
    readPendingQueue.emplace_back(data);

    schedule(eventReadDone, delay);
  }

  void readDone() {
    auto data = std::move(readPendingQueue.front());

    readPendingQueue.pop_front();
    postReadDone(data);

    if (readQueue.size() > 0) {
      submitRead();
    }
    else {
      readPending = false;
    }
  }

  void submitWrite() {
    auto data = std::move(writeQueue.front());

    writeQueue.pop_front();
    writePending = true;

    auto delay = preSubmitWrite(data);
    writePendingQueue.emplace_back(data);

    schedule(eventWriteDone, delay);
  }

  void writeDone() {
    auto data = std::move(writePendingQueue.front());

    writePendingQueue.pop_front();
    postWriteDone(data);

    if (writeQueue.size() > 0) {
      submitWrite();
    }
    else {
      writePending = false;
    }
  }

  preFunction preSubmitRead;
  preFunction preSubmitWrite;
  postFunction postReadDone;
  postFunction postWriteDone;

  backupFunction backupItem;
  restoreFunction restoreItem;

 public:
  Scheduler(ObjectData &o, std::string prefix, preFunction pr, preFunction pw,
            postFunction por, postFunction pow, backupFunction b,
            restoreFunction r)
      : Object(o),
        readPending(false),
        writePending(false),
        eventReadDone(InvalidEventID),
        eventWriteDone(InvalidEventID),
        preSubmitRead(pr),
        preSubmitWrite(pw),
        postReadDone(por),
        postWriteDone(pow),
        backupItem(b),
        restoreItem(r) {
    // Create events
    eventReadDone = createEvent([this](uint64_t) { readDone(); },
                                prefix + "::eventReadDone");
    eventWriteDone = createEvent([this](uint64_t) { writeDone(); },
                                 prefix + "::eventWriteDone");
  }

  virtual ~Scheduler() {}

  void read(Type data) {
    readQueue.emplace_back(data);

    if (!readPending) {
      submitRead();
    }
  }

  void write(Type data) {
    writeQueue.emplace_back(data);

    if (!writePending) {
      submitWrite();
    }
  }

  void getStatList(std::vector<Stat> &, std::string) noexcept override {}
  void getStatValues(std::vector<double> &) noexcept override {}
  void resetStatValues() noexcept override {}

  void createCheckpoint(std::ostream &out) const noexcept override {
    BACKUP_SCALAR(out, readPending);
    BACKUP_SCALAR(out, writePending);

    uint64_t size = readQueue.size();
    BACKUP_SCALAR(out, size);

    for (auto &iter : readQueue) {
      backupItem(out, iter);
    }

    size = readPendingQueue.size();
    BACKUP_SCALAR(out, size);

    for (auto &iter : readPendingQueue) {
      backupItem(out, iter);
    }

    size = writeQueue.size();
    BACKUP_SCALAR(out, size);

    for (auto &iter : writeQueue) {
      backupItem(out, iter);
    }

    size = writePendingQueue.size();
    BACKUP_SCALAR(out, size);

    for (auto &iter : writePendingQueue) {
      backupItem(out, iter);
    }
  }

  void restoreCheckpoint(std::istream &in) noexcept override {
    RESTORE_SCALAR(in, readPending);
    RESTORE_SCALAR(in, writePending);

    uint64_t size;
    RESTORE_SCALAR(in, size);

    for (uint64_t i = 0; i < size; i++) {
      auto item = restoreItem(in);

      readQueue.emplace_back(item);
    }

    RESTORE_SCALAR(in, size);

    for (uint64_t i = 0; i < size; i++) {
      auto item = restoreItem(in);

      readPendingQueue.emplace_back(item);
    }

    RESTORE_SCALAR(in, size);

    for (uint64_t i = 0; i < size; i++) {
      auto item = restoreItem(in);

      writeQueue.emplace_back(item);
    }

    RESTORE_SCALAR(in, size);

    for (uint64_t i = 0; i < size; i++) {
      auto item = restoreItem(in);

      writePendingQueue.emplace_back(item);
    }
  }
};

}  // namespace SimpleSSD

#endif
