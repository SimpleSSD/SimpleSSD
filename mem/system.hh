// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_MEM_MEMORY_SYSTEM_HH__
#define __SIMPLESSD_MEM_MEMORY_SYSTEM_HH__

#include <cinttypes>
#include <deque>

#include "cpu/cpu.hh"
#include "sim/log.hh"

namespace SimpleSSD {

struct ObjectData;

namespace Memory {

namespace SRAM {

class AbstractSRAM;

}

namespace DRAM {

class AbstractDRAM;

}

static const uint64_t MemoryPacketSize = 64;  // 64bytes request size

enum class MemoryType : uint8_t {
  SRAM,
  DRAM,
  Invalid,
};

class System {
 private:
  ObjectData *pobject;

  struct MemoryMap {
    uint64_t base;
    uint64_t size;

    std::string name;

    MemoryMap(std::string &&n, uint64_t b, uint64_t s)
        : base(b), size(s), name(std::move(n)) {}
  };

  SRAM::AbstractSRAM *sram;
  DRAM::AbstractDRAM *dram;

  uint64_t SRAMbaseAddress;
  uint64_t totalSRAMCapacity;
  uint64_t DRAMbaseAddress;
  uint64_t totalDRAMCapacity;

  std::vector<MemoryMap> allocatedAddressMap;

  struct MemoryRequest {
    uint64_t start;
    uint32_t npkt;

    bool read;
    bool sram;

    uint32_t submit;
    uint32_t complete;

    Event eid;
    uint64_t data;

    MemoryRequest()
        : start(0),
          npkt(0),
          read(false),
          sram(false),
          submit(0),
          complete(0),
          eid(InvalidEventID),
          data(0) {}
    MemoryRequest(bool r, bool s, uint64_t b, uint64_t e, Event ei, uint64_t d)
        : start(b), read(r), sram(s), submit(0), complete(0), eid(ei), data(d) {
      npkt = (uint32_t)((e - b) / MemoryPacketSize);
    }
  };

  uint64_t memoryTag;
  uint64_t lastTag;

  std::map<uint64_t, MemoryRequest> requestQueue;

  void breakRequest(bool, bool, uint64_t, uint32_t, Event, uint64_t);

  uint64_t dispatchPeriod;

  bool pending;

  void updateDispatch();

  Event eventDispatch;
  void dispatch();

  Event eventSRAMDone;
  Event eventDRAMDone;
  void completion(uint64_t, uint64_t);

  template <class... T>
  inline void debugprint(const char *format, T... args) noexcept;

  template <class... T>
  inline void warn_log(const char *format, T... args) noexcept;

  template <class... T>
  inline void panic_log(const char *format, T... args) noexcept;

  inline MemoryType validate(uint64_t offset, uint32_t size);

 public:
  System(ObjectData *);
  ~System();

  /**
   * \brief Read Memory
   *
   * Read Memory with callback event.
   *
   * \param[in] address   Begin address of SRAM
   * \param[in] length    Amount of data to read
   * \param[in] eid       Event ID of callback event
   * \param[in] data      Event data
   * \param[in] cacheable False for bypassing LLC (only for DRAM address)
   */
  void read(uint64_t address, uint32_t length, Event eid, uint64_t data = 0,
            bool cacheable = true);

  /**
   * \brief Write Memory
   *
   * Write Memory with callback event.
   *
   * \param[in] address   Begin address of SRAM
   * \param[in] length    Amount of data to write
   * \param[in] eid       Event ID of callback event
   * \param[in] data      Event data
   * \param[in] cacheable False for bypassing LLC (only for DRAM address)
   */
  void write(uint64_t address, uint32_t length, Event eid, uint64_t data = 0,
             bool cacheable = true);

  /**
   * \brief Allocate range of Memory
   *
   * Allocate a portion of memory address range. If no space available, it
   * panic. (You need to configure larger Memory for firmware.)
   * To check memory is available, set dry as true and check address is zero.
   *
   * \param[in] size  Requested memory size
   * \param[in] type  Memory type
   * \param[in] name  Description of memory range
   * \param[in] dry   Dry-run (to check memory is allocatable)
   * \return address  Beginning address of allocated range (dry = false)
   *                  Usable memory size when allocation failed (dry = true)
   *                  Zero when allocated successfully (dry = true)
   */
  uint64_t allocate(uint64_t size, MemoryType type, std::string &&name,
                    bool dry = false);

  void printMemoryLayout();

  void getStatList(std::vector<Stat> &, std::string) noexcept;
  void getStatValues(std::vector<double> &) noexcept;
  void resetStatValues() noexcept;

  void createCheckpoint(std::ostream &) const noexcept;
  void restoreCheckpoint(std::istream &) noexcept;
};

}  // namespace Memory

}  // namespace SimpleSSD

#endif
