// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "util/abstract_fifo.hh"

namespace SimpleSSD {

AbstractFIFO::AbstractFIFO(ObjectData &&o, std::string prefix)
    : Object(std::move(o)),
      readPending(false),
      writePending(false),
      eventReadDone(InvalidEventID),
      eventWriteDone(InvalidEventID) {
  // Create events
  eventReadDone = createEvent([this](uint64_t, void *c) { readDone(c); },
                              prefix + "::eventReadDone");
  eventWriteDone = createEvent([this](uint64_t, void *c) { writeDone(c); },
                               prefix + "::eventWriteDone");
}

AbstractFIFO::~AbstractFIFO() {}

void AbstractFIFO::submitRead() {
  auto data = std::move(readQueue.front());

  readQueue.pop();
  readPending = true;

  auto delay = preSubmitRead(data);

  schedule(eventReadDone, delay, data);
}

void AbstractFIFO::readDone(void *data) {
  postReadDone(data);

  if (readQueue.size() > 0) {
    submitRead();
  }
  else {
    readPending = false;
  }
}

void AbstractFIFO::submitWrite() {
  auto data = std::move(writeQueue.front());

  writeQueue.pop();
  writePending = true;

  auto delay = preSubmitWrite(data);

  schedule(eventWriteDone, delay, data);
}

void AbstractFIFO::writeDone(void *data) {
  postWriteDone(data);

  if (writeQueue.size() > 0) {
    submitWrite();
  }
  else {
    writePending = false;
  }
}

void AbstractFIFO::read(void *data) {
  readQueue.emplace(data);

  if (!readPending) {
    submitRead();
  }
}

void AbstractFIFO::write(void *data) {
  writeQueue.emplace(data);

  if (!writePending) {
    submitWrite();
  }
}

uint64_t AbstractFIFO::preSubmitRead(void *) {
  return 0;
}

uint64_t AbstractFIFO::preSubmitWrite(void *) {
  return 0;
}

void AbstractFIFO::postReadDone(void *) {}

void AbstractFIFO::postWriteDone(void *) {}

void AbstractFIFO::getStatList(std::vector<Stat> &, std::string) noexcept {}

void AbstractFIFO::getStatValues(std::vector<double> &) noexcept {}

void AbstractFIFO::resetStatValues() noexcept {}

}  // namespace SimpleSSD
