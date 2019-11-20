// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "mem/dram/controller.hh"

#include "mem/dram/ideal.hh"

namespace SimpleSSD::Memory::DRAM {

Channel::Channel(ObjectData &o, uint8_t i) : Object(o), id(i) {
  switch ((Memory::Config::Model)readConfigUint(
      Section::Memory, Memory::Config::Key::DRAMModel)) {
    case Memory::Config::Model::Ideal:
      pDRAM = new Memory::DRAM::Ideal(object);
      break;
    case Memory::Config::Model::LPDDR4:
      // pDRAM = new Memory::DRAM::TimingDRAM(object);
      break;
    default:
      std::cerr << "Invalid DRAM model selected." << std::endl;

      abort();
  }
}

Channel::~Channel() {
  delete pDRAM;
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
  values.push_back((double)readBytes);
  values.push_back((double)writeCount);
  values.push_back((double)writeMerged);
  values.push_back((double)writeBytes);
}

void Channel::resetStatValues() noexcept {
  readCount = 0;
  readFromWriteQueue = 0;
  readBytes = 0;
  writeCount = 0;
  writeMerged = 0;
  writeBytes = 0;
}

void Channel::createCheckpoint(std::ostream &out) const noexcept {
  pDRAM->createCheckpoint(out);
}

void Channel::restoreCheckpoint(std::istream &in) noexcept {
  pDRAM->restoreCheckpoint(in);
}

DRAMController::DRAMController(ObjectData &o) : AbstractRAM(o) {
  uint8_t nc = object.config->getDRAM()->channel;

  ctrl = object.config->getDRAMController();

  channels.resize(nc);

  // Create DRAM Objects
  for (uint8_t i = 0; i < nc; i++) {
    channels[i] = new Channel(object, i);
  }
}

DRAMController::~DRAMController() {
  for (auto &channel : channels) {
    delete channel;
  }
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
