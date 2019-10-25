// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "hil/nvme/queue.hh"

namespace SimpleSSD::HIL::NVMe {

SQEntry::SQEntry() {
  memset(data, 0, 64);
}

CQEntry::CQEntry() {
  memset(data, 0, 16);
}

Queue::Queue(ObjectData &o, DMAEngine *d)
    : Object(o),
      id(0),
      head(0),
      tail(0),
      size(0),
      stride(0),
      dmaEngine(d),
      dmaTag(InvalidDMATag) {}

Queue::Queue(ObjectData &o, DMAEngine *d, uint16_t qid, uint32_t length,
             uint32_t stride)
    : Object(o),
      id(qid),
      head(0),
      tail(0),
      size(length),
      stride(stride),
      dmaEngine(d),
      dmaTag(InvalidDMATag) {}

Queue::~Queue() {
  if (dmaTag != InvalidDMATag) {
    dmaEngine->deinit(dmaTag);
  }
}

void Queue::setDMAData(uint64_t base, bool pc, Event eid, uint64_t gcid) {
  if (pc) {
    dmaTag = dmaEngine->initRaw(base, (uint32_t)(size * stride));

    scheduleNow(eid, gcid);
  }
  else {
    dmaTag =
        dmaEngine->initFromPRP(0, base, (uint32_t)(size * stride), eid, gcid);
  }
}

uint16_t Queue::getID() {
  return id;
}

uint16_t Queue::getItemCount() {
  if (tail >= head) {
    return tail - head;
  }
  else {
    return (size - head) + tail;
  }
}

uint16_t Queue::getHead() {
  return head;
}

uint16_t Queue::getTail() {
  return tail;
}

uint16_t Queue::getSize() {
  return size;
}

void Queue::getStatList(std::vector<Stat> &, std::string) noexcept {}

void Queue::getStatValues(std::vector<double> &) noexcept {}

void Queue::resetStatValues() noexcept {}

void Queue::createCheckpoint(std::ostream &out) const noexcept {
  BACKUP_SCALAR(out, id);
  BACKUP_SCALAR(out, head);
  BACKUP_SCALAR(out, tail);
  BACKUP_SCALAR(out, size);
  BACKUP_SCALAR(out, stride);
  BACKUP_DMATAG(out, dmaTag);
}

void Queue::restoreCheckpoint(std::istream &in) noexcept {
  RESTORE_SCALAR(in, id);
  RESTORE_SCALAR(in, head);
  RESTORE_SCALAR(in, tail);
  RESTORE_SCALAR(in, size);
  RESTORE_SCALAR(in, stride);
  RESTORE_DMATAG(dmaEngine, in, dmaTag);
}

CQueue::CQueue(ObjectData &o, DMAEngine *d)
    : Queue(o, d), ien(false), phase(false), iv(0xFFFF) {}

CQueue::CQueue(ObjectData &o, DMAEngine *d, uint16_t qid, uint32_t length,
               uint32_t stride, uint16_t iv, bool en)
    : Queue(o, d, qid, length, stride), ien(en), phase(true), iv(iv) {}

void CQueue::setData(CQEntry *entry, Event eid) {
  if (entry) {
    // Set phase
    entry->dword3.status &= 0xFFFE;
    entry->dword3.status |= (phase ? 0x0001 : 0x0000);

    // Write entry
    dmaEngine->write(dmaTag, tail * stride, 0x10, entry->data, eid);

    // Increase tail
    tail++;

    if (tail == size) {
      tail = 0;
      phase = !phase;
    }

    panic_if(head == tail, "Completion queue overflow");
  }
}

uint16_t CQueue::incHead() {
  head++;

  if (head == size) {
    head = 0;
  }

  return head;
}

void CQueue::setHead(uint16_t newHead) {
  head = newHead;
}

bool CQueue::interruptEnabled() {
  return ien;
}

uint16_t CQueue::getInterruptVector() {
  return iv;
}

void CQueue::createCheckpoint(std::ostream &out) const noexcept {
  Queue::createCheckpoint(out);

  BACKUP_SCALAR(out, ien);
  BACKUP_SCALAR(out, phase);
  BACKUP_SCALAR(out, iv);
}

void CQueue::restoreCheckpoint(std::istream &in) noexcept {
  Queue::restoreCheckpoint(in);

  RESTORE_SCALAR(in, ien);
  RESTORE_SCALAR(in, phase);
  RESTORE_SCALAR(in, iv);
}

SQueue::SQueue(ObjectData &o, DMAEngine *d)
    : Queue(o, d), cqID(0xFFFF), priority(QueuePriority::Low) {}

SQueue::SQueue(ObjectData &o, DMAEngine *d, uint16_t qid, uint32_t length,
               uint32_t stride, uint16_t cqid, QueuePriority pri)
    : Queue(o, d, qid, length, stride), cqID(cqid), priority(pri) {}

uint16_t SQueue::getCQID() {
  return cqID;
}

void SQueue::setTail(uint16_t newTail) {
  tail = newTail;
}

void SQueue::getData(SQEntry *entry, Event eid) {
  if (entry && head != tail) {
    // Read entry
    dmaEngine->read(dmaTag, head * stride, 0x40, entry->data, eid);

    // Increase head
    head++;

    if (head == size) {
      head = 0;
    }
  }
}

QueuePriority SQueue::getPriority() {
  return priority;
}

void SQueue::createCheckpoint(std::ostream &out) const noexcept {
  Queue::createCheckpoint(out);

  BACKUP_SCALAR(out, cqID);
  BACKUP_SCALAR(out, priority);
}

void SQueue::restoreCheckpoint(std::istream &in) noexcept {
  Queue::restoreCheckpoint(in);

  RESTORE_SCALAR(in, cqID);
  RESTORE_SCALAR(in, priority);
}

}  // namespace SimpleSSD::HIL::NVMe
