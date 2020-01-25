// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "mem/system.hh"

#include "mem/dram/ideal/ideal.hh"
#include "mem/sram/sram.hh"
#include "sim/object.hh"
#include "util/algorithm.hh"

namespace SimpleSSD::Memory {

System::System(ObjectData *po)
    : cpu(po->cpu), config(po->config), log(po->log) {
  switch ((Config::Model)config->readUint(Section::Memory,
                                          Config::Key::DRAMModel)) {
    case Config::Model::Ideal:
      dram = new DRAM::Ideal::IdealDRAM(*po);

      break;
    default:
      panic("Unexpected DRAM model.");

      break;
  }

  DRAMbaseAddress = 0;
  totalDRAMCapacity = dram->size();

  sram = new SRAM::SRAM(*po);
  SRAMbaseAddress = totalDRAMCapacity;
  totalSRAMCapacity = config->getSRAM()->size;
}

System::~System() {
  delete sram;
  delete dram;
}

void System::read(uint64_t address, uint32_t length, Event eid, uint64_t data,
                  bool cacheable) {
  auto type = validate(address, length);

  if (UNLIKELY(type == MemoryType::Invalid)) {
    panic_log("Invalid memory read from %" PRIX64 "h + %Xh", address, length);
  }

  if (type == MemoryType::SRAM) {
    address -= SRAMbaseAddress;
    // TODO: Access SRAM here
  }
  else {
    address -= DRAMbaseAddress;
    // TODO: Access DRAM here
  }
}

void System::write(uint64_t address, uint32_t length, Event eid, uint64_t data,
                   bool cacheable) {
  auto type = validate(address, length);

  if (UNLIKELY(type == MemoryType::Invalid)) {
    panic_log("Invalid memory write to %" PRIX64 "h + %Xh", address, length);
  }

  if (type == MemoryType::SRAM) {
    address -= SRAMbaseAddress;
    // TODO: Access SRAM here
  }
  else {
    address -= DRAMbaseAddress;
    // TODO: Access DRAM here
  }
}

uint64_t System::allocate(uint64_t size, MemoryType type, std::string &&name,
                          bool dry) {
  if (UNLIKELY(type >= MemoryType::Invalid)) {
    panic_log("Invalid memory type %u.", (uint8_t)type);
  }

  uint64_t unallocated =
      type == MemoryType::DRAM ? totalDRAMCapacity : totalSRAMCapacity;
  uint64_t lastBase = 0;

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
    panic_log("%" PRIu64 " bytes requested, but %" PRIu64 "bytes left in DRAM.",
              size, unallocated);
  }

  allocatedAddressMap.emplace_back(std::move(name), lastBase, size);

  return lastBase;
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

  sram->createCheckpoint(out);
  dram->createCheckpoint(out);
}

void System::restoreCheckpoint(std::istream &in) noexcept {
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

  sram->restoreCheckpoint(in);
  dram->restoreCheckpoint(in);
}

}  // namespace SimpleSSD::Memory
