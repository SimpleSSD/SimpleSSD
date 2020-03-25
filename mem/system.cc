// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "mem/system.hh"

#include "mem/dram/ideal/ideal.hh"
#include "mem/dram/simple/simple.hh"
#include "mem/sram/sram.hh"
#include "sim/object.hh"
#include "util/algorithm.hh"

namespace SimpleSSD::Memory {

System::System(ObjectData *po)
    : pobject(po), memoryTag(0), lastTag(0), pending(false) {
  // Create memories
  switch ((Config::Model)pobject->config->readUint(Section::Memory,
                                                   Config::Key::DRAMModel)) {
    case Config::Model::Ideal:
      dram = new DRAM::Ideal::IdealDRAM(*pobject);

      break;
    case Config::Model::Simple:
      dram = new DRAM::Simple::SimpleDRAM(*pobject);

      break;
    default:
      panic("Unexpected DRAM model.");

      break;
  }

  DRAMbaseAddress = 0;
  totalDRAMCapacity = dram->size();

  sram = new SRAM::SRAM(*pobject);
  SRAMbaseAddress = totalDRAMCapacity;
  totalSRAMCapacity = pobject->config->getSRAM()->size;

  // Calculate dispatch period
  auto clock =
      pobject->config->readUint(Section::Memory, Config::Key::SystemBusSpeed);

  dispatchPeriod = 1000000000000ull / clock;

  panic_if(dispatchPeriod == 0, "System bus is too fast (period == 0).");

  eventDispatch =
      pobject->cpu->createEvent([this](uint64_t, uint64_t) { dispatch(); },
                                "Memory::System::eventDispatch");
  eventSRAMDone = pobject->cpu->createEvent(
      [this](uint64_t t, uint64_t d) { completion(t, d); },
      "Memory::System::eventSRAMDone");
  eventDRAMDone = pobject->cpu->createEvent(
      [this](uint64_t t, uint64_t d) { completion(t, d); },
      "Memory::System::eventDRAMDone");
}

System::~System() {
  delete sram;
  delete dram;
}

inline void System::debugprint(const char *format, ...) noexcept {
  va_list args;

  va_start(args, format);
  pobject->log->debugprint(Log::DebugID::Memory, format, args);
  va_end(args);
}

inline void System::warn_log(const char *format, ...) noexcept {
  va_list args;

  va_start(args, format);
  pobject->log->print(Log::LogID::Warn, format, args);
  va_end(args);
}

inline void System::panic_log(const char *format, ...) noexcept {
  va_list args;

  va_start(args, format);
  pobject->log->print(Log::LogID::Panic, format, args);
  va_end(args);
}

inline MemoryType System::validate(uint64_t offset, uint32_t size) {
  if (offset >= SRAMbaseAddress &&
      offset + size <= SRAMbaseAddress + totalSRAMCapacity) {
    return MemoryType::SRAM;
  }
  else if (offset >= DRAMbaseAddress &&
           offset + size <= DRAMbaseAddress + totalDRAMCapacity) {
    return MemoryType::DRAM;
  }

  return MemoryType::Invalid;
}

void System::breakRequest(bool read, bool sram, uint64_t address,
                          uint32_t length, Event eid, uint64_t data) {
  uint64_t tag = memoryTag++;

  uint64_t alignedBegin = (address / MemoryPacketSize) * MemoryPacketSize;
  uint64_t alignedEnd =
      DIVCEIL(address + length, MemoryPacketSize) * MemoryPacketSize;

  requestQueue.emplace(
      tag, MemoryRequest(read, sram, alignedBegin, alignedEnd, eid, data));
}

void System::updateDispatch() {
  if (!pending) {
    pending = true;

    pobject->cpu->schedule(eventDispatch);
  }
}

void System::dispatch() {
  if (!requestQueue.empty()) {
    // We have request
    auto last = requestQueue.find(lastTag);

    panic_if(last == requestQueue.end(), "");

    // Submit last
    auto &iter = last->second;
    uint64_t addr = iter.start + MemoryPacketSize * iter.submit++;

    if (iter.sram) {
      if (iter.read) {
        sram->read(addr, eventSRAMDone, lastTag);
      }
      else {
        sram->write(addr, eventSRAMDone, lastTag);
      }
    }
    else {
      if (iter.read) {
        dram->read(addr, eventDRAMDone, lastTag);
      }
      else {
        dram->write(addr, eventDRAMDone, lastTag);
      }
    }

    if (iter.submit == iter.npkt) {
      lastTag++;
      last++;
    }

    if (last != requestQueue.end()) {
      pobject->cpu->schedule(eventDispatch, 0, dispatchPeriod);

      return;
    }
  }

  // Dispatch done
  pending = false;

  return;
}

void System::completion(uint64_t now, uint64_t tag) {
  auto iter = requestQueue.find(tag);

  panic_if(iter == requestQueue.end(), "Unexpected completion.");

  iter->second.complete++;

  if (iter->second.complete == iter->second.npkt) {
    // Completed
    pobject->cpu->scheduleAbs(iter->second.eid, iter->second.data, now);

    requestQueue.erase(iter);
  }
}

void System::read(uint64_t address, uint32_t length, Event eid, uint64_t data,
                  bool) {
  auto type = validate(address, length);

  if (UNLIKELY(type == MemoryType::Invalid)) {
    panic_log("Invalid memory read from %" PRIX64 "h + %Xh", address, length);
  }

  if (type == MemoryType::SRAM) {
    address -= SRAMbaseAddress;

    breakRequest(true, true, address, length, eid, data);
  }
  else {
    address -= DRAMbaseAddress;

    breakRequest(true, false, address, length, eid, data);
  }

  updateDispatch();
}

void System::write(uint64_t address, uint32_t length, Event eid, uint64_t data,
                   bool) {
  auto type = validate(address, length);

  if (UNLIKELY(type == MemoryType::Invalid)) {
    panic_log("Invalid memory write to %" PRIX64 "h + %Xh", address, length);
  }

  if (type == MemoryType::SRAM) {
    address -= SRAMbaseAddress;

    breakRequest(false, true, address, length, eid, data);
  }
  else {
    address -= DRAMbaseAddress;

    breakRequest(false, false, address, length, eid, data);
  }

  updateDispatch();
}

uint64_t System::allocate(uint64_t size, MemoryType type, std::string &&name,
                          bool dry) {
  if (UNLIKELY(type >= MemoryType::Invalid)) {
    panic_log("Invalid memory type %u.", (uint8_t)type);
  }

  uint64_t unallocated =
      type == MemoryType::DRAM ? totalDRAMCapacity : totalSRAMCapacity;
  uint64_t lastBase =
      type == MemoryType::DRAM ? DRAMbaseAddress : SRAMbaseAddress;

  for (auto &iter : allocatedAddressMap) {
    if (validate(iter.base, iter.size) == type) {
      unallocated -= iter.size;

      if (lastBase < iter.base + iter.size) {
        lastBase = iter.base + iter.size;
      }
    }
  }

  if (dry) {
    return unallocated < size ? unallocated : 0;
  }

  if (unallocated < size) {
    // Print current memory map
    uint64_t i = 0;

    for (auto &iter : allocatedAddressMap) {
      warn_log("%" PRIu64 ": %" PRIx64 "h + %" PRIx64 "h: %s", i, iter.base,
               iter.size, iter.name.c_str());
    }

    // Panic
    panic_log("%" PRIu64 " bytes requested, but %" PRIu64
              " bytes left in DRAM.",
              size, unallocated);
  }

  allocatedAddressMap.emplace_back(std::move(name), lastBase, size);

  return lastBase;
}

void System::printMemoryLayout() {
  uint64_t sramInUse = 0;
  uint64_t dramInUse = 0;

  debugprint("Memory map information:");
  debugprint(" Type |     Base     |     Size     | Name");

  for (auto &iter : allocatedAddressMap) {
    auto ret = validate(iter.base, iter.size);

    if (ret == MemoryType::SRAM) {
      sramInUse += iter.size;

      debugprint(" SRAM | %12" PRIX64 " | %12" PRIX64 " | %s", iter.base,
                 iter.size, iter.name.c_str());
    }
    else if (ret == MemoryType::DRAM) {
      dramInUse += iter.size;

      debugprint(" DRAM | %12" PRIX64 " | %12" PRIX64 " | %s", iter.base,
                 iter.size, iter.name.c_str());
    }
  }

  debugprint("Memory utilization:");
  debugprint(" SRAM: %" PRIX64 "h / %" PRIX64 "h", sramInUse,
             totalSRAMCapacity);
  debugprint(" DRAM: %" PRIX64 "h / %" PRIX64 "h", dramInUse,
             totalDRAMCapacity);
}

void System::getStatList(std::vector<Stat> &list, std::string prefix) noexcept {
  sram->getStatList(list, prefix + "sram.");
  dram->getStatList(list, prefix + "dram.");
}

void System::getStatValues(std::vector<double> &values) noexcept {
  sram->getStatValues(values);
  dram->getStatValues(values);
}

void System::resetStatValues() noexcept {
  sram->resetStatValues();
  dram->resetStatValues();
}

void System::createCheckpoint(std::ostream &out) const noexcept {
  BACKUP_SCALAR(out, SRAMbaseAddress);
  BACKUP_SCALAR(out, totalSRAMCapacity);
  BACKUP_SCALAR(out, DRAMbaseAddress);
  BACKUP_SCALAR(out, totalDRAMCapacity);

  uint64_t size = allocatedAddressMap.size();

  BACKUP_SCALAR(out, size);

  for (auto &iter : allocatedAddressMap) {
    size = iter.name.length();
    BACKUP_SCALAR(out, size);

    BACKUP_BLOB(out, iter.name.data(), size);

    BACKUP_SCALAR(out, iter.base);
    BACKUP_SCALAR(out, iter.size);
  }

  BACKUP_SCALAR(out, memoryTag);

  size = requestQueue.size();

  BACKUP_SCALAR(out, size);

  for (auto &iter : requestQueue) {
    BACKUP_SCALAR(out, iter.first);
    BACKUP_SCALAR(out, iter.second.start);
    BACKUP_SCALAR(out, iter.second.npkt);
    BACKUP_SCALAR(out, iter.second.read);
    BACKUP_SCALAR(out, iter.second.sram);
    BACKUP_SCALAR(out, iter.second.submit);
    BACKUP_SCALAR(out, iter.second.complete);
    BACKUP_EVENT(out, iter.second.eid);
    BACKUP_SCALAR(out, iter.second.data);
  }

  BACKUP_SCALAR(out, pending);

  BACKUP_EVENT(out, eventDispatch);
  BACKUP_EVENT(out, eventSRAMDone);
  BACKUP_EVENT(out, eventDRAMDone);

  sram->createCheckpoint(out);
  dram->createCheckpoint(out);
}

void System::restoreCheckpoint(std::istream &in) noexcept {
  // For macro
  ObjectData &object = *pobject;

  RESTORE_SCALAR(in, SRAMbaseAddress);
  RESTORE_SCALAR(in, totalSRAMCapacity);
  RESTORE_SCALAR(in, DRAMbaseAddress);
  RESTORE_SCALAR(in, totalDRAMCapacity);

  uint64_t size;

  RESTORE_SCALAR(in, size);

  allocatedAddressMap.reserve(size);

  for (uint64_t i = 0; i < size; i++) {
    uint64_t l, a, s;
    std::string name;

    RESTORE_SCALAR(in, l);
    name.resize(l);

    RESTORE_BLOB(in, name.data(), l);

    RESTORE_SCALAR(in, a);
    RESTORE_SCALAR(in, s);

    allocatedAddressMap.emplace_back(std::move(name), a, s);
  }

  RESTORE_SCALAR(in, memoryTag);

  RESTORE_SCALAR(in, size);

  for (uint64_t i = 0; i < size; i++) {
    uint64_t tag;
    MemoryRequest req;

    RESTORE_SCALAR(in, tag);
    RESTORE_SCALAR(in, req.start);
    RESTORE_SCALAR(in, req.npkt);
    RESTORE_SCALAR(in, req.read);
    RESTORE_SCALAR(in, req.sram);
    RESTORE_SCALAR(in, req.submit);
    RESTORE_SCALAR(in, req.complete);
    RESTORE_EVENT(in, req.eid);
    RESTORE_SCALAR(in, req.data);

    requestQueue.emplace(tag, req);
  }

  RESTORE_SCALAR(in, pending);

  RESTORE_EVENT(in, eventDispatch);
  RESTORE_EVENT(in, eventSRAMDone);
  RESTORE_EVENT(in, eventDRAMDone);

  sram->restoreCheckpoint(in);
  dram->restoreCheckpoint(in);
}

}  // namespace SimpleSSD::Memory
