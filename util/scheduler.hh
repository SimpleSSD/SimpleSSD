// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __UTIL_SCHEDULER_HH__
#define __UTIL_SCHEDULER_HH__

#include <queue>

#include "sim/object.hh"

namespace SimpleSSD {

template <class Type, std::enable_if_t<std::is_pointer_v<Type>> * = nullptr>
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

  std::queue<Type> readQueue;
  std::queue<Type *> writeQueue;

  void submitRead() {
    auto data = std::move(readQueue.front());

    readQueue.pop();
    readPending = true;

    auto delay = preSubmitRead(data);

    schedule(eventReadDone, delay, data);
  }

  void readDone(Type data) {
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

    writeQueue.pop();
    writePending = true;

    auto delay = preSubmitWrite(data);

    schedule(eventWriteDone, delay, data);
  }

  void writeDone(Type) {
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
    eventReadDone = createEvent(
        [this](uint64_t, EventContext c) { readDone(c.get<Type>()); },
        prefix + "::eventReadDone");
    eventWriteDone = createEvent(
        [this](uint64_t, EventContext c) { writeDone(c.get<Type>()); },
        prefix + "::eventWriteDone");
  }

  virtual ~Scheduler() {}

  void read(Type data) {
    readQueue.emplace(data);

    if (!readPending) {
      submitRead();
    }
  }

  void write(Type data) {
    writeQueue.emplace(data);

    if (!writePending) {
      submitWrite();
    }
  }

  void getStatList(std::vector<Stat> &, std::string) noexcept override {}
  void getStatValues(std::vector<double> &) noexcept override {}
  void resetStatValues() noexcept override {}

  void createCheckpoint(std::ostream &) noexcept override {
    BACKUP_SCALAR(out, readPending);
    BACKUP_SCALAR(out, writePending);

    uint64_t size = readQueue.size();
    BACKUP_SCALAR(out, size);

    for (uint64_t i = 0; i < size; i++) {
      void *item = readQueue.front();

      backupItem(out, item);

      readQueue.pop();
    }

    size = writeQueue.size();
    BACKUP_SCALAR(out, size);

    for (uint64_t i = 0; i < size; i++) {
      void *item = writeQueue.front();

      backupItem(out, item);

      writeQueue.pop();
    }
  }

  void restoreCheckpoint(std::istream &) noexcept override {
    RESTORE_SCALAR(in, readPending);
    RESTORE_SCALAR(in, writePending);

    uint64_t size;
    RESTORE_SCALAR(in, size);

    for (uint64_t i = 0; i < size; i++) {
      void *item = restoreItem(in);

      readQueue.push(item);
    }

    RESTORE_SCALAR(in, size);

    for (uint64_t i = 0; i < size; i++) {
      void *item = restoreItem(in);

      writeQueue.push(item);
    }
  }
};

}  // namespace SimpleSSD

#endif
