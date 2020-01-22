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
    : cpu(e), config(c), log(l), totalCapacity(0) {}

System::~System() {}

uint64_t System::allocate(uint64_t size, std::string &&name, bool dry) {
  uint64_t ret = 0;
  uint64_t unallocated = totalCapacity;

  if (UNLIKELY(unallocated == 0)) {
    panic_log("Unexpected memory capacity.");
  }

  for (auto &iter : addressMap) {
    unallocated -= iter.size;
  }

  if (dry) {
    return unallocated < size ? unallocated : 0;
  }

  if (unallocated < size) {
    // Print current memory map
    uint64_t i = 0;

    for (auto &iter : addressMap) {
      warn_log("%" PRIu64 ": %" PRIx64 "h + %" PRIx64 "h: %s", i, iter.base,
               iter.size, iter.name.c_str());
    }

    // Panic
    panic_log("%" PRIu64 " bytes requested, but %" PRIu64 "bytes left in DRAM.",
              size, unallocated);
  }

  if (addressMap.size() > 0) {
    ret = addressMap.back().base + addressMap.back().size;
  }

  addressMap.emplace_back(std::move(name), ret, size);

  return ret;
}

void System::getStatList(std::vector<Stat> &, std::string) noexcept {}

void System::getStatValues(std::vector<double> &) noexcept {}

void System::resetStatValues() noexcept {}

void System::createCheckpoint(std::ostream &out) const noexcept {
  BACKUP_SCALAR(out, totalCapacity);

  uint64_t size = addressMap.size();

  BACKUP_SCALAR(out, size);

  for (auto &iter : addressMap) {
    size = iter.name.length();
    BACKUP_SCALAR(out, size);

    BACKUP_BLOB(out, iter.name.data(), size);

    BACKUP_SCALAR(out, iter.base);
    BACKUP_SCALAR(out, iter.size);
  }
}

void System::restoreCheckpoint(std::istream &in) noexcept {
  RESTORE_SCALAR(in, totalCapacity);

  uint64_t size;

  RESTORE_SCALAR(in, size);

  addressMap.reserve(size);

  for (uint64_t i = 0; i < size; i++) {
    uint64_t l, a, s;
    std::string name;

    RESTORE_SCALAR(in, l);
    name.resize(l);

    RESTORE_BLOB(in, name.data(), l);

    RESTORE_SCALAR(in, a);
    RESTORE_SCALAR(in, s);

    addressMap.emplace_back(std::move(name), a, s);
  }
}

}  // namespace SimpleSSD::Memory
