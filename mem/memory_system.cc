// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "mem/memory_system.hh"

#include "util/algorithm.hh"

namespace SimpleSSD::Memory {

System::System(CPU::CPU *e, ConfigReader *c, Log *l)
    : cpu(e), config(c), log(l) {
  // TODO: Create SRAM object
  SRAMbaseAddress = 0;
  totalSRAMCapacity = 0;

  // TODO: Create DRAM object
  DRAMbaseAddress = 0;
  totalDRAMCapacity = 0;
}

System::~System() {}

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

void System::getStatList(std::vector<Stat> &, std::string) noexcept {}

void System::getStatValues(std::vector<double> &) noexcept {}

void System::resetStatValues() noexcept {}

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
}

}  // namespace SimpleSSD::Memory
