// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_MEM_ABSTRACT_RAM_HH__
#define __SIMPLESSD_MEM_ABSTRACT_RAM_HH__

#include "sim/object.hh"

namespace SimpleSSD::Memory {

class AbstractRAM : public Object {
 protected:
  struct MemoryMap {
    std::string name;
    uint64_t base;
    uint64_t size;

    MemoryMap(std::string &&n, uint64_t b, uint64_t s)
        : name(std::move(n)), base(b), size(s) {}
  };

  uint64_t totalCapacity;
  std::vector<MemoryMap> addressMap;

 public:
  AbstractRAM(ObjectData &o) : Object(o), totalCapacity(0) {}
  virtual ~AbstractRAM() {}

  /**
   * \brief Read SRAM
   *
   * Read SRAM with callback event.
   *
   * \param[in] address Begin address of SRAM
   * \param[in] length  Amount of data to read
   * \param[in] eid     Event ID of callback event
   * \param[in] data    Event data
   */
  virtual void read(uint64_t address, uint64_t length, Event eid,
                    uint64_t data = 0) = 0;

  /**
   * \brief Write SRAM
   *
   * Write SRAM with callback event.
   *
   * \param[in] address Begin address of SRAM
   * \param[in] length  Amount of data to write
   * \param[in] eid     Event ID of callback event
   * \param[in] data    Event data
   */
  virtual void write(uint64_t address, uint64_t length, Event eid,
                     uint64_t data = 0) = 0;

  /**
   * \brief Allocate range of RAM
   *
   * Allocate a portion of memory address range. If no space available, it
   * panic. (You need to configure larger RAM for firmware.)
   * To check memory is available, set dry as true and check address is zero.
   *
   * \param[in] size  Requested memory size
   * \param[in] name  Description of memory range
   * \param[in] dry   Dry-run (to check memory is allocatable)
   * \return address  Beginning address of allocated range
   */
  virtual uint64_t allocate(uint64_t size, std::string &&name,
                            bool dry = false) {
    uint64_t ret = 0;
    uint64_t unallocated = totalCapacity;

    panic_if(unallocated == 0, "Unexpected memory capacity.");

    for (auto &iter : addressMap) {
      unallocated -= iter.size;
    }

    if (dry) {
      return unallocated < size ? unallocated : 0;
    }

    panic_if(unallocated < size,
             "%" PRIu64 " bytes requested, but %" PRIu64 "bytes left in DRAM.",
             size, unallocated);

    if (addressMap.size() > 0) {
      ret = addressMap.back().base + addressMap.back().size;
    }

    addressMap.emplace_back(std::move(name), ret, size);

    return ret;
  }

  void createCheckpoint(std::ostream &out) const noexcept override {
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

  void restoreCheckpoint(std::istream &in) noexcept override {
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
};

class Request {
 public:
  uint64_t offset;
  uint64_t length;
  Event eid;
  uint64_t data;
  uint64_t beginAt;

  Request() : offset(0), length(0), eid(InvalidEventID) {}
  Request(uint64_t a, uint64_t l, Event e, uint64_t d)
      : offset(a), length(l), eid(e), data(d) {}
  Request(const Request &) = delete;
  Request(Request &&) noexcept = default;

  Request &operator=(const Request &) = delete;
  Request &operator=(Request &&) = default;

  static void backup(std::ostream &out, Request *item) {
    BACKUP_EVENT(out, item->eid);
    BACKUP_SCALAR(out, item->offset);
    BACKUP_SCALAR(out, item->length);
    BACKUP_SCALAR(out, item->beginAt);
  }

  static Request *restore(std::istream &in, ObjectData &object) {
    auto item = new Request();

    RESTORE_EVENT(in, item->eid);
    RESTORE_SCALAR(in, item->offset);
    RESTORE_SCALAR(in, item->length);
    RESTORE_SCALAR(in, item->beginAt);

    return item;
  }
};

}  // namespace SimpleSSD::Memory

#endif
