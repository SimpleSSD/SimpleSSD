// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "mem/dram/controller.hh"

#include "mem/dram/ideal.hh"

#define QUEUE_HIT_LATENCY ((uint64_t)10000)  // 10ns

namespace SimpleSSD::Memory::DRAM {

Channel::Channel(ObjectData &o, DRAMController *p, uint8_t i, uint32_t e)
    : Object(o),
      id(i),
      decodeAddress(p->getDecodeFunction()),
      entrySize(e),
      isInRead(true),
      writeCount(0),
      internalEntryID(0) {
  ctrl = object.config->getDRAMController();

  switch ((Memory::Config::Model)readConfigUint(
      Section::Memory, Memory::Config::Key::DRAMModel)) {
    case Memory::Config::Model::Ideal:
      pDRAM = new Memory::DRAM::Ideal(object);
      break;
    case Memory::Config::Model::LPDDR4:
      // pDRAM = new Memory::DRAM::DRAMController(object);
      break;
    default:
      panic("Invalid DRAM model selected.");
  }

  // Scheduler
  switch (ctrl->schedulePolicy) {
    case Memory::Config::MemoryScheduling::FCFS:
      chooseNext =
          [this](std::list<Entry> &queue) -> std::list<Entry>::iterator {
        auto iter = queue.begin();

        for (; iter != queue.end(); ++iter) {
          Address addr = decodeAddress(iter->address);

          if (pDRAM->isNotRefresh(addr.rank, addr.bank)) {
            break;
          }
        }

        if (iter == queue.end() && queue.size() > 0) {
          iter = queue.begin();
        }

        return iter;
      };

      break;
    case Memory::Config::MemoryScheduling::FRFCFS:
      chooseNext =
          [this](std::list<Entry> &queue) -> std::list<Entry>::iterator {
        auto iter = queue.begin();

        for (; iter != queue.end(); ++iter) {
          Address addr = decodeAddress(iter->address);

          if (pDRAM->isNotRefresh(addr.rank, addr.bank) &&
              pDRAM->getRowInfo(addr.rank, addr.bank) == addr.row) {
            break;
          }
        }

        if (iter == queue.end()) {
          iter = queue.begin();

          for (; iter != queue.end(); ++iter) {
            Address addr = decodeAddress(iter->address);

            if (pDRAM->isNotRefresh(addr.rank, addr.bank)) {
              break;
            }
          }
        }

        if (iter == queue.end() && queue.size() > 0) {
          iter = queue.begin();
        }

        return iter;
      };

      break;
    default:
      panic("Invalid scheduling policy.");

      break;
  }

  // Make event
  eventDoNext = createEvent(
      [this](uint64_t, uint64_t) { submitRequest(); },
      "Memory::DRAM::Channel<" + std::to_string(i) + ">::eventDoNext");
  eventReadDone = createEvent(
      [this](uint64_t, uint64_t d) { completeRequest(d, true); },
      "Memory::DRAM::Channel<" + std::to_string(i) + ">::eventReadDone");
  eventWriteDone = createEvent(
      [this](uint64_t, uint64_t d) { completeRequest(d, false); },
      "Memory::DRAM::Channel<" + std::to_string(i) + ">::eventWriteDone");
}

Channel::~Channel() {
  delete pDRAM;
}

void Channel::submitRequest() {
  bool switchState = false;

  if (isInRead) {
    if (readRequestQueue.size() > 0) {
      // Choose request
      auto iter = chooseNext(readRequestQueue);

      if (iter != readRequestQueue.end()) {
        // Submit to DRAM
        pDRAM->submit(decodeAddress(iter->address), entrySize, true,
                      eventReadDone, internalEntryID);

        // Remove request from queue
        responseQueue.emplace(internalEntryID++, *iter);
        readRequestQueue.erase(iter);

        // Switch to write?
        if (writeQueue.size() >=
            ctrl->writeMaxThreshold * ctrl->writeQueueSize) {
          switchState = true;
        }
      }
    }
    else if (writeQueue.size() > 0) {
      switchState = true;
    }
  }
  else {
    // Choose request
    auto iter = chooseNext(writeRequestQueue);

    if (iter != writeRequestQueue.end()) {
      // Submit to DRAM
      pDRAM->submit(decodeAddress(iter->address), entrySize, false,
                    eventWriteDone, internalEntryID);

      // Remove request from queue
      auto aiter = writeQueue.find(iter->address);
      writeQueue.erase(aiter);

      responseQueue.emplace(internalEntryID++, *iter);
      writeRequestQueue.erase(iter);

      // Switch to read?
      if (writeQueue.size() == 0 ||
          (readRequestQueue.size() > 0 && writeCount >= ctrl->minWriteBurst)) {
        switchState = true;
      }
    }
  }

  if (switchState) {
    if (isInRead) {
      writeCount = 0;
    }

    isInRead = !isInRead;
  }

  if (!isScheduled(eventDoNext)) {
    scheduleRel(eventDoNext, 0, 0);
  }
}

void Channel::completeRequest(uint64_t id, bool read) {
  auto iter = responseQueue.find(id);

  panic_if(iter == responseQueue.end(), "Invalid DRAM response.");

  if (read) {
    scheduleNow(iter->second.event, iter->second.data);
  }

  responseQueue.erase(iter);
}

uint8_t Channel::addToReadQueue(uint64_t addr, Event eid, uint64_t data) {
  bool foundInWrQ = false;

  if (writeQueue.find(addr) != writeQueue.end()) {
    foundInWrQ = true;

    readFromWriteQueue++;
  }

  if (!foundInWrQ) {
    readRequestQueue.emplace_back(addr, eid, data);
  }
  else {
    scheduleNow(eid, data);
  }

  if (!isScheduled(eventDoNext)) {
    scheduleNow(eventDoNext);
  }

  return true;
}

uint8_t Channel::addToWriteQueue(uint64_t addr) {
  bool merged = writeQueue.find(addr) != writeQueue.end();

  if (!merged) {
    writeQueue.emplace(addr);
    writeRequestQueue.emplace_back(addr, InvalidEventID, 0);
  }
  else {
    writeMerged++;
  }

  if (!isScheduled(eventDoNext)) {
    scheduleNow(eventDoNext);
  }

  return 0;
}

uint8_t Channel::submit(uint64_t addr, bool read, Event eid, uint64_t data) {
  uint64_t now = getTick();

  if (!read) {
    if (writeRequestQueue.size() < ctrl->writeQueueSize) {
      writeCount++;

      return addToWriteQueue(addr);
    }
  }
  else {
    if (readRequestQueue.size() < ctrl->readQueueSize) {
      readCount++;

      return addToReadQueue(addr, eid, data);
    }
  }

  return 2;
}

void Channel::getStatList(std::vector<Stat> &list,
                          std::string prefix) noexcept {
  list.emplace_back(prefix + "request_count.read.total",
                    "Number of read requests");
  list.emplace_back(prefix + "request_count.read.fromWriteQueue",
                    "Number of DRAM read serviced by the write queue");
  list.emplace_back(prefix + "request_count.write.total",
                    "Number of write requests");
  list.emplace_back(prefix + "request_count.write.merge",
                    "Number of DRAM write bursts merged with an existing one");
  list.emplace_back(prefix + "byte.read.total",
                    "Total number of bytes read from DRAM");
  list.emplace_back(prefix + "byte.read.fromWriteQueue",
                    "Total number of bytes read from write queue");
  list.emplace_back(prefix + "byte.write.total",
                    "Total number of bytes written to DRAM");
}

void Channel::getStatValues(std::vector<double> &values) noexcept {
  values.push_back((double)readCount);
  values.push_back((double)readFromWriteQueue);
  values.push_back((double)(readCount * entrySize));
  values.push_back((double)writeCount);
  values.push_back((double)writeMerged);
  values.push_back((double)(writeCount * entrySize));
}

void Channel::resetStatValues() noexcept {
  readCount = 0;
  readFromWriteQueue = 0;
  writeCount = 0;
  writeMerged = 0;
}

void Channel::createCheckpoint(std::ostream &out) const noexcept {
  pDRAM->createCheckpoint(out);
}

void Channel::restoreCheckpoint(std::istream &in) noexcept {
  pDRAM->restoreCheckpoint(in);
}

DRAMController::DRAMController(ObjectData &o)
    : AbstractRAM(o), internalEntryID(0), internalSubentryID(0) {
  auto dram = object.config->getDRAM();
  uint8_t nc = dram->channel;

  ctrl = object.config->getDRAMController();

  channels.resize(nc);

  entrySize = (uint64_t)dram->width * dram->burstChop / 8;

  // Create DRAM Objects
  for (uint8_t i = 0; i < nc; i++) {
    channels[i] = new Channel(object, this, i, entrySize);
  }

  // Calculate maximums
  addressLimit.channel = nc;
  addressLimit.rank = dram->rank;
  addressLimit.bank = dram->bank;
  addressLimit.row = dram->chipSize / dram->bank / dram->rowSize;

  capacity = dram->chipSize * dram->chip * dram->rank * dram->channel;

  panic_if(popcount64(entrySize) != 1,
           "Memory request size should be power of 2.");
  warn_if(entrySize != 64, "Memory request size is not 64 bytes.");

  // Panic
  panic_if(popcount32(dram->bank) != 1, "# Bank should be power of 2.");
  panic_if(popcount32(dram->rowSize) != 1,
           "Row buffer size should be power of 2.");

  // Make address decoder
  switch (ctrl->addressPolicy) {
    case Config::AddressMapping::RoRaBaChCo:
      decodeAddress = [channel = addressLimit.channel, rank = addressLimit.rank,
                       bank = addressLimit.bank, row = addressLimit.row,
                       col = dram->rowSize](uint64_t addr) -> Address {
        Address ret;

        addr = addr / col;
        ret.channel = addr % channel;
        addr = addr / channel;
        ret.bank = addr % bank;
        addr = addr / bank;
        ret.rank = addr % rank;
        addr = addr / rank;
        ret.row = addr % row;

        return ret;
      };

      break;
    case Config::AddressMapping::RoRaBaCoCh:
      decodeAddress = [channel = addressLimit.channel, rank = addressLimit.rank,
                       bank = addressLimit.bank, row = addressLimit.row,
                       col = dram->rowSize](uint64_t addr) -> Address {
        Address ret;

        ret.channel = addr % channel;
        addr = addr / channel;
        addr = addr / col;
        ret.bank = addr % bank;
        addr = addr / bank;
        ret.rank = addr % rank;
        addr = addr / rank;
        ret.row = addr % row;

        return ret;
      };

      break;
    case Config::AddressMapping::RoCoRaBaCh:
      decodeAddress = [channel = addressLimit.channel, rank = addressLimit.rank,
                       bank = addressLimit.bank, row = addressLimit.row,
                       col = dram->rowSize](uint64_t addr) -> Address {
        Address ret;

        ret.channel = addr % channel;
        addr = addr / channel;
        ret.bank = addr % bank;
        addr = addr / bank;
        ret.rank = addr % rank;
        addr = addr / rank;
        addr = addr / col;
        ret.row = addr % row;

        return ret;
      };

      break;
  }

  // Make events
  eventReadRetry = createEvent([this](uint64_t, uint64_t) { readRetry(); },
                               "Memory::DRAM::TimingDRAM::eventReadRetry");
  eventWriteRetry = createEvent([this](uint64_t, uint64_t) { writeRetry(); },
                                "Memory::DRAM::TimingDRAM::eventWriteRetry");
  eventReadComplete = createEvent(
      [this](uint64_t t, uint64_t d) { completeRequest(t, d, true); },
      "Memory::DRAM::TimingDRAM::eventReadComplete");
  eventWriteComplete = createEvent(
      [this](uint64_t t, uint64_t d) { completeRequest(t, d, false); },
      "Memory::DRAM::TimingDRAM::eventWriteComplete");
}

DRAMController::~DRAMController() {
  for (auto &channel : channels) {
    delete channel;
  }
}

std::function<Address(uint64_t)> &DRAMController::getDecodeFunction() {
  return decodeAddress;
}

void DRAMController::readRetry() {
  auto req = readRetryQueue.front();

  uint8_t ret = channels[decodeAddress(req->address).channel]->submit(
      req->address, true, eventReadComplete, req->id);

  if (ret == 2) {
    // Queue full
    return;
  }

  // Request submitted
  readRetryQueue.pop_front();

  // Check write queue hit (complete immediately)
  if (ret == 1) {
    // We may have multiple read completion at same time
    readCompletionQueue.emplace_back(req->id);

    if (!isScheduled(eventReadComplete)) {
      scheduleRel(eventReadComplete, req->id, QUEUE_HIT_LATENCY);
    }
  }

  // Schedule next retry
  if (readRetryQueue.size() > 0) {
    scheduleNow(eventReadRetry);
  }
}

void DRAMController::writeRetry() {
  auto req = writeRetryQueue.front();

  uint8_t ret = channels[decodeAddress(req->address).channel]->submit(
      req->address, false, eventWriteComplete, req->id);

  if (ret == 0) {
    // Request submitted - write is async
    writeRetryQueue.pop_front();

    // We may have multiple write completion at same time
    writeCompletionQueue.emplace_back(req->id);

    if (!isScheduled(eventWriteComplete)) {
      scheduleRel(eventWriteComplete, req->id, QUEUE_HIT_LATENCY);
    }

    // Schedule next retry
    if (writeRetryQueue.size() > 0) {
      scheduleNow(eventWriteRetry);
    }
  }
  else {
    // Queue full
  }
}

void DRAMController::submitRequest(uint64_t addr, uint32_t size, bool read,
                                   Event eid, uint64_t data) {
  // Split request
  uint64_t alignedBegin = addr / entrySize;
  uint64_t alignedEnd = alignedBegin + DIVCEIL(size, entrySize) * entrySize;

  // Find statistic bin
  StatisticBin *stat = nullptr;

  for (auto &iter : statbin) {
    if (iter.inRange(addr, size)) {
      stat = &iter;

      break;
    }
  }

  // Create request
  Entry req(internalEntryID, (alignedEnd - alignedBegin) / entrySize, eid, data,
            stat);

  auto ret = entries.emplace(internalEntryID++, req);

  // Generate splitted requests
  std::list<SubEntry *> *target = nullptr;

  if (read) {
    target = &readRetryQueue;
  }
  else {
    target = &writeRetryQueue;
  }

  for (uint64_t addr = alignedBegin; addr < alignedEnd; addr += entrySize) {
    auto sret = subentries.emplace(
        internalSubentryID,
        SubEntry(internalSubentryID, &ret.first->second, addr));

    target->emplace_back(&sret.first->second);
  }

  // Try submit
  if (read && readRetryQueue.size() > 1 && !isScheduled(eventReadRetry)) {
    readRetry();
  }
  else if (!read && writeRetryQueue.size() > 1 &&
           !isScheduled(eventWriteRetry)) {
    writeRetry();
  }
}

void DRAMController::completeRequest(uint64_t now, uint64_t id, bool read) {
  auto iter = subentries.find(id);

  panic_if(iter == subentries.end(), "Unexpected subentry ID %" PRIu64 ".", id);

  // Get latency
  double latency = (now - iter->second.submitted) / 1000.0;

  // Addup stat
  auto parent = iter->second.parent;

  if (read) {
    parent->stat->readCount++;
    parent->stat->readBytes += entrySize;
    parent->stat->readLatencySum += latency;
  }
  else {
    parent->stat->writeCount++;
    parent->stat->writeBytes += entrySize;
    parent->stat->writeLatencySum += latency;
  }

  // Check completion
  parent->counter++;

  if (parent->counter == parent->chunks) {
    // All splitted requests are completed
    scheduleNow(parent->eid, parent->data);

    entries.erase(entries.find(parent->id));
  }

  // Remove me
  subentries.erase(iter);

  // Retry request
  if (read) {
    readCompletionQueue.pop_front();

    // Complete next write request
    if (readCompletionQueue.size() > 0) {
      scheduleNow(eventReadComplete, readCompletionQueue.front());
    }
  }
  else {
    writeCompletionQueue.pop_front();

    // Complete next write request
    if (writeCompletionQueue.size() > 0) {
      scheduleNow(eventWriteComplete, writeCompletionQueue.front());
    }
  }
}

void DRAMController::read(uint64_t addr, uint32_t size, Event eid,
                          uint64_t data) {
  submitRequest(addr, size, true, eid, data);
}

void DRAMController::write(uint64_t addr, uint32_t size, Event eid,
                           uint64_t data) {
  submitRequest(addr, size, false, eid, data);
}

uint64_t DRAMController::allocate(uint64_t size, std::string &&name, bool dry) {
  uint64_t address = AbstractRAM::allocate(size, std::move(name), dry);

  if (!dry) {
    statbin.emplace_back(StatisticBin(address, size));
  }

  return address;
}

void DRAMController::getStatList(std::vector<Stat> &list,
                                 std::string prefix) noexcept {
  for (uint8_t i = 0; i < channels.size(); i++) {
    channels[i]->getStatList(list,
                             prefix + "channel" + std::to_string(i) + ".");
  }

  // Per-memory bin statistics
  uint32_t idx = 0;

  for (auto &stat : statbin) {
    stat.getStatList(list, prefix + "range" + std::to_string(idx++) + ".");
  }
}

void DRAMController::getStatValues(std::vector<double> &values) noexcept {
  uint64_t now = getTick();

  for (auto &channel : channels) {
    channel->getStatValues(values);
  }

  for (auto &stat : statbin) {
    stat.getStatValues(values, now);
  }
}

void DRAMController::resetStatValues() noexcept {
  uint64_t now = getTick();

  for (auto &channel : channels) {
    channel->resetStatValues();
  }

  for (auto &stat : statbin) {
    stat.resetStatValues(now);
  }
}

void DRAMController::createCheckpoint(std::ostream &out) const noexcept {
  AbstractRAM::createCheckpoint(out);

  uint8_t t8 = (uint8_t)channels.size();

  BACKUP_SCALAR(out, t8);

  for (auto &channel : channels) {
    channel->createCheckpoint(out);
  }
}

void DRAMController::restoreCheckpoint(std::istream &in) noexcept {
  AbstractRAM::restoreCheckpoint(in);

  uint8_t t8;

  RESTORE_SCALAR(in, t8);

  panic_if(t8 != channels.size(), "DRAM channel mismatch while restore.");

  for (auto &channel : channels) {
    channel->restoreCheckpoint(in);
  }
}

DRAMController::StatisticBin::StatisticBin(uint64_t b, uint64_t e)
    : begin(b), end(e) {
  resetStatValues(0);
}

bool DRAMController::StatisticBin::inRange(uint64_t addr, uint64_t length) {
  if (begin <= addr && addr + length <= end) {
    return true;
  }

  return false;
}

void DRAMController::StatisticBin::getStatList(std::vector<Stat> &list,
                                               std::string prefix) noexcept {
  // Count
  list.emplace_back(prefix + "request_count.read", "Read request count");
  list.emplace_back(prefix + "request_count.write", "Write request count");
  list.emplace_back(prefix + "request_count.total", "Total request count");

  // Capacity
  list.emplace_back(prefix + "bytes.read", "Read data size in bytes");
  list.emplace_back(prefix + "bytes.write", "Write data size in bytes");
  list.emplace_back(prefix + "bytes.total", "Total data size in bytes");

  // Bandwidth
  list.emplace_back(prefix + "bandwidth.read", "Read bandwidth in MiB/s");
  list.emplace_back(prefix + "bandwidth.write", "Write bandwidth in MiB/s");
  list.emplace_back(prefix + "bandwidth.total", "Total bandwidth in MiB/s");

  // Latency
  list.emplace_back(prefix + "bandwidth.read", "Average read latency in ns");
  list.emplace_back(prefix + "bandwidth.write", "Average write latency in ns");
  list.emplace_back(prefix + "bandwidth.total", "Average latency in ns");
}

void DRAMController::StatisticBin::getStatValues(std::vector<double> &values,
                                                 uint64_t now) noexcept {
  values.push_back((double)readCount);
  values.push_back((double)writeCount);
  values.push_back((double)(readCount + writeCount));

  values.push_back((double)readBytes);
  values.push_back((double)writeBytes);
  values.push_back((double)(readBytes + writeBytes));

  double period = (now - lastResetAt) / 1000000000000.0;

  values.push_back((double)readBytes / 1048576.0 / period);
  values.push_back((double)writeBytes / 1048576.0 / period);
  values.push_back((double)(readBytes + writeBytes) / 1048576.0 / period);

  values.push_back(readLatencySum / readCount);
  values.push_back(writeLatencySum / writeCount);
  values.push_back((readLatencySum + writeLatencySum) /
                   (readCount + writeCount));
}

void DRAMController::StatisticBin::resetStatValues(uint64_t now) noexcept {
  lastResetAt = now;

  lastResetAt = 0;
  readCount = 0;
  writeCount = 0;
  readBytes = 0;
  writeBytes = 0;
  readLatencySum = 0.;
  writeLatencySum = 0.;
}

}  // namespace SimpleSSD::Memory::DRAM
