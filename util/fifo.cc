/*
 * Copyright (C) 2017 CAMELab
 *
 * This file is part of SimpleSSD.
 *
 * SimpleSSD is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * SimpleSSD is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with SimpleSSD.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "util/fifo.hh"

#include "util/algorithm.hh"

namespace SimpleSSD {

FIFOEntry::FIFOEntry()
    : addr(0),
      size(0),
      buffer(nullptr),
      arrivedAt(0),
      insertBeginAt(0),
      insertEndAt(0),
      context(nullptr) {}

FIFOEntry::FIFOEntry(uint64_t a, uint64_t s, uint8_t *b, DMAFunction &f,
                     void *c)
    : addr(a),
      size(s),
      buffer(b),
      arrivedAt(getTick()),
      insertBeginAt(0),
      insertEndAt(0),
      func(f),
      context(c) {}

FIFO::Queue::Queue(uint64_t c)
    : capacity(c), usage(0), insertPending(false), transferPending(false) {}

FIFO::FIFO(DMAInterface *up, FIFOParam &p)
    : upstream(up), param(p), readQueue(p.rqSize), writeQueue(p.wqSize) {
  // Set Transfer Unit Latency
  unitLatency = param.latency(param.transferUnit);

  // Allocate events and functions for each queue
  readQueue.insertDone = allocate([this](uint64_t) { insertDone(readQueue); });
  readQueue.beginTransfer =
      allocate([this](uint64_t) { beginTransfer(readQueue, true); });
  readQueue.submitCompletion =
      allocate([this](uint64_t) { transferDoneNext(readQueue, true); });
  readQueue.transferDone = [this](uint64_t, void *) {
    transferDone(readQueue, true);
  };
  writeQueue.insertDone =
      allocate([this](uint64_t) { insertDone(writeQueue); });
  writeQueue.beginTransfer =
      allocate([this](uint64_t) { beginTransfer(writeQueue, false); });
  writeQueue.submitCompletion =
      allocate([this](uint64_t) { transferDoneNext(writeQueue, false); });
  writeQueue.transferDone = [this](uint64_t, void *) {
    transferDone(writeQueue, false);
  };
}

FIFO::~FIFO() {}

void FIFO::beginInsert(FIFO::Queue &queue) {
  auto &iter = queue.waitQueue.front();
  uint64_t now = getTick();

  if (queue.insertPending || queue.usage + iter.size > queue.capacity) {
    return;
  }

  queue.insertPending = true;

  // Current item will be written to FIFO
  queue.usage += iter.size;
  iter.insertBeginAt = now;
  iter.insertEndAt = now + param.latency(iter.size);

  // When insertion done, call handler
  schedule(queue.insertDone, iter.insertEndAt);

  // Current item will be transferred after one transferUnit written to FIFO
  // If we already scheduled, current item will be transffered after last one
  if (!scheduled(queue.beginTransfer)) {
    schedule(queue.beginTransfer, now + unitLatency);
  }

  // Push current item to transferQueue
  queue.transferQueue.push(iter);
}

void FIFO::insertDone(FIFO::Queue &queue) {
  // Pop last item
  queue.waitQueue.pop();
  queue.insertPending = false;

  // Insert next waiting item
  if (queue.waitQueue.size() > 0) {
    beginInsert(queue);
  }
}

void FIFO::beginTransfer(FIFO::Queue &queue, bool isRead) {
  auto &iter = queue.transferQueue.front();

  if (queue.transferPending) {
    return;
  }

  queue.transferPending = true;

  // Begin transfer
  if (isRead) {
    upstream->dmaRead(iter.addr, iter.size, iter.buffer, queue.transferDone);
  }
  else {
    upstream->dmaWrite(iter.addr, iter.size, iter.buffer, queue.transferDone);
  }
}

void FIFO::transferDone(FIFO::Queue &queue, bool isRead) {
  auto &iter = queue.transferQueue.front();
  uint64_t now = getTick();

  if (now >= iter.insertEndAt + unitLatency) {  // Upstream <= downstream
    // Finish immediately
    transferDoneNext(queue, isRead);
  }
  else {  // upstream is faster than downstream
    // Should finish at insertedEndAt + upstreamLatency(transferUnit)
    // TODO: Applying unitLatency is not correct (more slower result)
    schedule(queue.submitCompletion, iter.insertEndAt + unitLatency);
  }
}

void FIFO::transferDoneNext(FIFO::Queue &queue, bool isRead) {
  auto &iter = queue.transferQueue.front();
  uint64_t now = getTick();

  // Call handler
  iter.func(now, iter.context);

  // Transfer done
  queue.usage -= iter.size;
  queue.transferPending = false;
  queue.transferQueue.pop();

  // Do next transfer if we have
  if (queue.transferQueue.size() > 0) {
    iter = queue.transferQueue.front();

    if (now > iter.insertBeginAt + unitLatency) {
      beginTransfer(queue, isRead);
    }
  }

  // Do next insertion if we have
  if (queue.waitQueue.size() > 0) {
    beginInsert(queue);
  }
}

void FIFO::dmaRead(uint64_t addr, uint64_t size, uint8_t *buffer,
                   DMAFunction &func, void *context) {
  readQueue.waitQueue.push(FIFOEntry(addr, size, buffer, func, context));

  beginInsert(readQueue);
}

void FIFO::dmaWrite(uint64_t addr, uint64_t size, uint8_t *buffer,
                    DMAFunction &func, void *context) {
  writeQueue.waitQueue.push(FIFOEntry(addr, size, buffer, func, context));

  beginInsert(writeQueue);
}

}  // namespace SimpleSSD
