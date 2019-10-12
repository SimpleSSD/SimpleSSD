// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "util/fifo.hh"

#include "util/algorithm.hh"

namespace SimpleSSD {

FIFOEntry::FIFOEntry()
    : last(true),
      id(0),
      addr(0),
      size(0),
      buffer(nullptr),
      arrivedAt(0),
      insertBeginAt(0),
      insertEndAt(0),
      eid(InvalidEventID) {}

FIFOEntry::FIFOEntry(uint64_t a, uint64_t s, uint8_t *b, uint64_t t, Event e)
    : last(true),
      id(0),
      addr(a),
      size(s),
      buffer(b),
      arrivedAt(t),
      insertBeginAt(0),
      insertEndAt(0),
      eid(e) {}

ReadEntry::ReadEntry() : id(0), insertEndAt(0), dmaEndAt(0), latency(0) {}

ReadEntry::ReadEntry(uint64_t a, uint64_t i, uint64_t d, uint64_t l)
    : id(a), insertEndAt(i), dmaEndAt(d), latency(l) {}

FIFO::Queue::Queue(uint64_t c)
    : capacity(c), usage(0), insertPending(false), transferPending(false) {}

void FIFO::Queue::backup(std::ostream &out) const noexcept {
  BACKUP_SCALAR(out, capacity);
  BACKUP_SCALAR(out, usage);
  BACKUP_SCALAR(out, insertPending);
  BACKUP_SCALAR(out, transferPending);

  BACKUP_EVENT(out, insertDone);
  BACKUP_EVENT(out, beginTransfer);
  BACKUP_EVENT(out, submitCompletion);
  BACKUP_EVENT(out, transferDone);

  uint64_t size = waitQueue.size();
  BACKUP_SCALAR(out, size);

  for (auto &iter : waitQueue) {
    BACKUP_SCALAR(out, iter.last);
    BACKUP_SCALAR(out, iter.id);
    BACKUP_SCALAR(out, iter.addr);
    BACKUP_SCALAR(out, iter.size);
    BACKUP_SCALAR(out, iter.arrivedAt);
    BACKUP_SCALAR(out, iter.insertBeginAt);
    BACKUP_SCALAR(out, iter.insertEndAt);
    BACKUP_EVENT(out, iter.eid);
    BACKUP_BLOB(out, iter.buffer, iter.size);
  }

  size = transferQueue.size();
  BACKUP_SCALAR(out, size);

  for (auto &iter : transferQueue) {
    BACKUP_SCALAR(out, iter.last);
    BACKUP_SCALAR(out, iter.id);
    BACKUP_SCALAR(out, iter.addr);
    BACKUP_SCALAR(out, iter.size);
    BACKUP_SCALAR(out, iter.arrivedAt);
    BACKUP_SCALAR(out, iter.insertBeginAt);
    BACKUP_SCALAR(out, iter.insertEndAt);
    BACKUP_EVENT(out, iter.eid);
    BACKUP_BLOB(out, iter.buffer, iter.size);
  }
}

void FIFO::Queue::restore(std::istream &in, ObjectData &object) noexcept {
  RESTORE_SCALAR(in, capacity);
  RESTORE_SCALAR(in, usage);
  RESTORE_SCALAR(in, insertPending);
  RESTORE_SCALAR(in, transferPending);

  RESTORE_EVENT(in, insertDone);
  RESTORE_EVENT(in, beginTransfer);
  RESTORE_EVENT(in, submitCompletion);
  RESTORE_EVENT(in, transferDone);

  uint64_t size;
  RESTORE_SCALAR(in, size);

  for (uint64_t i = 0; i < size; i++) {
    waitQueue.push_back(FIFOEntry());

    RESTORE_SCALAR(in, waitQueue.back().last);
    RESTORE_SCALAR(in, waitQueue.back().id);
    RESTORE_SCALAR(in, waitQueue.back().addr);
    RESTORE_SCALAR(in, waitQueue.back().size);
    RESTORE_SCALAR(in, waitQueue.back().arrivedAt);
    RESTORE_SCALAR(in, waitQueue.back().insertBeginAt);
    RESTORE_SCALAR(in, waitQueue.back().insertEndAt);
    RESTORE_EVENT(in, waitQueue.back().eid);
    RESTORE_BLOB(in, waitQueue.back().buffer, waitQueue.back().size);
  }

  RESTORE_SCALAR(in, size);

  for (uint64_t i = 0; i < size; i++) {
    transferQueue.push_back(FIFOEntry());

    RESTORE_SCALAR(in, transferQueue.back().last);
    RESTORE_SCALAR(in, transferQueue.back().id);
    RESTORE_SCALAR(in, transferQueue.back().addr);
    RESTORE_SCALAR(in, transferQueue.back().size);
    RESTORE_SCALAR(in, transferQueue.back().arrivedAt);
    RESTORE_SCALAR(in, transferQueue.back().insertBeginAt);
    RESTORE_SCALAR(in, transferQueue.back().insertEndAt);
    RESTORE_EVENT(in, waitQueue.back().eid);
    RESTORE_BLOB(in, transferQueue.back().buffer, transferQueue.back().size);
  }
}

FIFO::FIFO(ObjectData &o, DMAInterface *up, FIFOParam &p)
    : Object(o),
      upstream(up),
      param(p),
      readQueue(p.rqSize),
      writeQueue(p.wqSize),
      counter(0) {
  // Sanity check
  panic_if(
      param.transferUnit > param.rqSize || param.transferUnit > param.wqSize,
      "Invalid transferUnit size");

  // Set Transfer Unit Latency
  unitLatency = param.latency(param.transferUnit);

  // Allocate events and functions for each queue
  writeQueue.insertDone =
      createEvent([this](uint64_t, uint64_t) { insertWriteDone(); },
                  "FIFO::writeQueue::insertDone");
  writeQueue.beginTransfer =
      createEvent([this](uint64_t, uint64_t) { transferWrite(); },
                  "FIFO::writeQueue::beginTransfer");
  writeQueue.submitCompletion =
      createEvent([this](uint64_t, uint64_t) { transferWriteDoneNext(); },
                  "FIFO::writeQueue::submitCompletion");
  writeQueue.transferDone =
      createEvent([this](uint64_t, uint64_t) { transferWriteDone(); },
                  "FIFO::writeQueue::transferDone");
  readQueue.insertDone =
      createEvent([this](uint64_t, uint64_t) { insertReadDone(); },
                  "FIFO::readQueue::insertDone");
  readQueue.beginTransfer =
      createEvent([this](uint64_t, uint64_t) { insertRead(); },
                  "FIFO::readQueue::beginTransfer");
  readQueue.submitCompletion =
      createEvent([this](uint64_t, uint64_t) { insertReadDoneNext(); },
                  "FIFO::readQueue::submitCompletion");
  readQueue.transferDone =
      createEvent([this](uint64_t, uint64_t) { transferReadDone(); },
                  "FIFO::readQueue::transferDone");
}

FIFO::~FIFO() {}

std::list<ReadEntry>::iterator FIFO::find(uint64_t id) {
  auto iter = readCompletion.begin();

  for (; iter != readCompletion.end(); ++iter) {
    if (iter->id == id) {
      break;
    }
  }

  return iter;
}

uint64_t FIFO::calcSize(uint64_t size, bool &b) {
  if (size < param.transferUnit) {
    b = true;

    return size;
  }

  return DIVCEIL(size, param.transferUnit) * param.transferUnit;
}

void FIFO::insertWrite() {
  auto iter = writeQueue.waitQueue.begin();
  bool smallerThanUnit = false;
  uint64_t now = getTick();
  uint64_t size = calcSize(iter->size, smallerThanUnit);

  if ((writeQueue.insertPending ||
       writeQueue.usage + size > writeQueue.capacity) &&
      size <= param.wqSize) {
    return;
  }

  // We need to split large request
  if (size > param.wqSize) {
    FIFOEntry copy = *iter;

    // copy is next one
    copy.last = true;
    copy.addr = iter->addr + param.transferUnit;
    copy.size = iter->size - param.transferUnit;
    copy.buffer = iter->buffer ? iter->buffer + param.transferUnit : nullptr;

    // iter is current one
    iter->last = false;
    iter->size = param.transferUnit;

    // Insert copy after iter
    writeQueue.waitQueue.insert(++iter, copy);

    // iter should first one
    iter = writeQueue.waitQueue.begin();
    size = calcSize(iter->size, smallerThanUnit);
  }

  writeQueue.insertPending = true;

  // Current item will be written to FIFO
  writeQueue.usage += size;
  iter->insertBeginAt = now;
  iter->insertEndAt = param.latency(size);

  // When insertion done, call handler
  schedule(writeQueue.insertDone, iter->insertEndAt);
  iter->insertEndAt += now;

  // Current item will be transferred after one transferUnit written to FIFO
  // If we alwritey scheduled, current item will be transffered after last one
  if (!isScheduled(writeQueue.beginTransfer)) {
    schedule(writeQueue.beginTransfer,
             (smallerThanUnit ? param.latency(size) : unitLatency));
  }

  // Push current item to transferQueue
  writeQueue.transferQueue.push_back(*iter);
}

void FIFO::insertWriteDone() {
  // Pop last item
  writeQueue.waitQueue.pop_front();
  writeQueue.insertPending = false;

  // Insert next waiting item
  if (writeQueue.waitQueue.size() > 0) {
    insertWrite();
  }
}

void FIFO::transferWrite() {
  auto &iter = writeQueue.transferQueue.front();

  if (writeQueue.transferPending) {
    return;
  }

  writeQueue.transferPending = true;

  // Begin transfer
  upstream->write(iter.addr, iter.size, iter.buffer, writeQueue.transferDone);
}

void FIFO::transferWriteDone() {
  auto &iter = writeQueue.transferQueue.front();
  uint64_t now = getTick();
  uint64_t latency =
      (iter.size < param.transferUnit) ? param.latency(iter.size) : unitLatency;

  if (now >= iter.insertEndAt + latency) {  // Upstream <= downstream
    // Finish immediately
    transferWriteDoneNext();
  }
  else {  // upstream is faster than downstream
    // Should finish at insertedEndAt + upstreamLatency(transferUnit)
    // TODO: Applying unitLatency is not correct (more slower result)
    schedule(writeQueue.submitCompletion, iter.insertEndAt + latency - now);
  }
}

void FIFO::transferWriteDoneNext() {
  auto &iter = writeQueue.transferQueue.front();
  bool unused;

  // Call handler
  if (iter.last) {
    schedule(iter.eid);
  }

  // Transfer done
  writeQueue.usage -= calcSize(iter.size, unused);
  writeQueue.transferPending = false;
  writeQueue.transferQueue.pop_front();

  // Do next transfer if we have
  if (writeQueue.transferQueue.size() > 0) {
    if (!isScheduled(writeQueue.beginTransfer)) {
      transferWrite();
    }
  }

  // Do next insertion if we have
  if (writeQueue.waitQueue.size() > 0) {
    insertWrite();
  }
}

void FIFO::transferRead() {
  auto iter = readQueue.waitQueue.begin();
  bool smallerThanUnit = false;
  uint64_t size = calcSize(iter->size, smallerThanUnit);

  if ((readQueue.transferPending ||
       readQueue.usage + size > readQueue.capacity) &&
      size <= param.rqSize) {
    return;
  }

  // We need to split large request
  if (size > param.rqSize) {
    FIFOEntry copy = *iter;

    // copy is next one
    copy.last = true;
    copy.addr = iter->addr + param.transferUnit;
    copy.size = iter->size - param.transferUnit;
    copy.buffer = iter->buffer ? iter->buffer + param.transferUnit : nullptr;

    // iter is current one
    iter->last = false;
    iter->size = param.transferUnit;

    // Insert copy after iter
    readQueue.waitQueue.insert(++iter, copy);

    // iter should first one
    iter = readQueue.waitQueue.begin();
    size = calcSize(iter->size, smallerThanUnit);
  }

  readQueue.transferPending = true;

  // Set ID
  iter->id = counter++;

  // Current item will be requested to be read
  readQueue.usage += size;

  // When all data is read to FIFO, call handler
  upstream->read(iter->addr, iter->size, iter->buffer, readQueue.transferDone);

  if (!isScheduled(readQueue.beginTransfer)) {
    schedule(readQueue.beginTransfer,
             (smallerThanUnit ? param.latency(size) : unitLatency));
  }

  // Push current item to transferQueue
  readQueue.transferQueue.push_back(*iter);
}

void FIFO::transferReadDone() {
  auto &iter = readQueue.waitQueue.front();
  uint64_t latency =
      (iter.size < param.transferUnit) ? param.latency(iter.size) : unitLatency;

  // Save DMA end time
  auto comp = find(iter.id);

  if (comp == readCompletion.end()) {
    readCompletion.push_back(ReadEntry(iter.id, 0, getTick(), latency));
  }
  else {
    comp->dmaEndAt = getTick();

    insertReadDoneMerge(comp);
  }

  // Pop last item
  readQueue.waitQueue.pop_front();
  readQueue.transferPending = false;

  // Transfer next waiting item
  if (readQueue.waitQueue.size() > 0) {
    transferRead();
  }
}

void FIFO::insertRead() {
  auto &iter = readQueue.transferQueue.front();
  bool unused;
  uint64_t now = getTick();

  if (readQueue.insertPending) {
    return;
  }

  readQueue.insertPending = true;

  // Begin insertion
  iter.insertBeginAt = now;
  iter.insertEndAt = param.latency(calcSize(iter.size, unused));

  // When insertion done, call handler
  if (!isScheduled(readQueue.insertDone)) {
    schedule(readQueue.insertDone, iter.insertEndAt);
  }

  iter.insertEndAt += now;
}

void FIFO::insertReadDone() {
  auto &iter = readQueue.transferQueue.front();
  uint64_t latency =
      (iter.size < param.transferUnit) ? param.latency(iter.size) : unitLatency;

  auto comp = find(iter.id);

  if (comp == readCompletion.end()) {
    readCompletion.push_back(ReadEntry(iter.id, getTick(), 0, latency));
  }
  else {
    insertReadDoneMerge(comp);
  }
}

void FIFO::insertReadDoneMerge(std::list<ReadEntry>::iterator comp) {
  uint64_t now = getTick();

  if (now >= comp->dmaEndAt + comp->latency) {  // Upstream <= downstream
    // Finish immediately
    insertReadDoneNext();
  }
  else {  // upstream is faster than downstream
    // Should finish at insertedEndAt + upstreamLatency(transferUnit)
    // TODO: Applying unitLatency is not correct (more slower result)
    schedule(readQueue.submitCompletion, comp->dmaEndAt + comp->latency - now);
  }

  readCompletion.erase(comp);
}

void FIFO::insertReadDoneNext() {
  auto &iter = readQueue.transferQueue.front();
  bool unused;

  // Call handler
  if (iter.last) {
    schedule(iter.eid);
  }

  // Insert done
  readQueue.usage -= calcSize(iter.size, unused);
  readQueue.insertPending = false;
  readQueue.transferQueue.pop_front();

  // Do next insert if we have
  if (readQueue.transferQueue.size() > 0) {
    if (!isScheduled(readQueue.beginTransfer)) {
      insertRead();
    }
  }

  // Do next transfer if we have
  if (readQueue.waitQueue.size() > 0) {
    transferRead();
  }
}

void FIFO::read(uint64_t addr, uint64_t size, uint8_t *buffer, Event eid) {
  if (size == 0) {
    warn("FIFO: zero-size DMA read request. Ignore.");

    return;
  }

  readQueue.waitQueue.push_back(FIFOEntry(addr, size, buffer, getTick(), eid));

  transferRead();
}

void FIFO::write(uint64_t addr, uint64_t size, uint8_t *buffer, Event eid) {
  if (size == 0) {
    warn("FIFO: zero-size DMA write request. Ignore.");

    return;
  }

  writeQueue.waitQueue.push_back(FIFOEntry(addr, size, buffer, getTick(), eid));

  insertWrite();
}

void FIFO::getStatList(std::vector<Stat> &, std::string) noexcept {}

void FIFO::getStatValues(std::vector<double> &) noexcept {}

void FIFO::resetStatValues() noexcept {}

void FIFO::createCheckpoint(std::ostream &out) const noexcept {
  BACKUP_SCALAR(out, param.rqSize);
  BACKUP_SCALAR(out, param.wqSize);
  BACKUP_SCALAR(out, param.transferUnit);
  BACKUP_SCALAR(out, unitLatency);

  readQueue.backup(out);
  writeQueue.backup(out);

  BACKUP_SCALAR(out, counter);

  uint64_t size = readCompletion.size();
  BACKUP_SCALAR(out, size);

  for (auto &iter : readCompletion) {
    BACKUP_SCALAR(out, iter.id);
    BACKUP_SCALAR(out, iter.insertEndAt);
    BACKUP_SCALAR(out, iter.dmaEndAt);
    BACKUP_SCALAR(out, iter.latency);
  }
}

void FIFO::restoreCheckpoint(std::istream &in) noexcept {
  RESTORE_SCALAR(in, param.rqSize);
  RESTORE_SCALAR(in, param.wqSize);
  RESTORE_SCALAR(in, param.transferUnit);
  RESTORE_SCALAR(in, unitLatency);

  readQueue.restore(in, object);
  writeQueue.restore(in, object);

  RESTORE_SCALAR(in, counter);

  uint64_t size;
  RESTORE_SCALAR(in, size);

  for (uint64_t i = 0; i < size; i++) {
    readCompletion.push_back(ReadEntry());

    RESTORE_SCALAR(in, readCompletion.back().id);
    RESTORE_SCALAR(in, readCompletion.back().insertEndAt);
    RESTORE_SCALAR(in, readCompletion.back().dmaEndAt);
    RESTORE_SCALAR(in, readCompletion.back().latency);
  }
}

}  // namespace SimpleSSD
