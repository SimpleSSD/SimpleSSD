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

#include "cpu/cpu.hh"
#include "sim/log.hh"

namespace SimpleSSD::Memory {

enum class MemoryType : uint8_t {
  SRAM,
  DRAM,
  Invalid,
};

class System {
 private:
  CPU::CPU *cpu;
  ConfigReader *config;
  Log *log;

  struct MemoryMap {
    uint64_t base;
    uint64_t size;

    std::string name;

    MemoryMap(std::string &&n, uint64_t b, uint64_t s)
        : base(b), size(s), name(std::move(n)) {}
  };

  // TODO: Add object of DRAM and SRAM

  uint64_t SRAMbaseAddress;
  uint64_t totalSRAMCapacity;
  uint64_t DRAMbaseAddress;
  uint64_t totalDRAMCapacity;

  std::vector<MemoryMap> allocatedAddressMap;

  inline void warn_log(const char *format, ...) noexcept {
    va_list args;

    va_start(args, format);
    log->print(Log::LogID::Warn, format, args);
    va_end(args);
  }

  inline void panic_log(const char *format, ...) noexcept {
    va_list args;

    va_start(args, format);
    log->print(Log::LogID::Panic, format, args);
    va_end(args);
  }

  inline MemoryType validate(uint64_t offset, uint32_t size) {
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

 public:
  System(CPU::CPU *, ConfigReader *, Log *);
  ~System();

  /**
   * \brief Read Memory
   *
   * Read Memory with callback event.
   *
   * \param[in] address Begin address of SRAM
   * \param[in] length  Amount of data to read
   * \param[in] eid     Event ID of callback event
   * \param[in] data    Event data
   */
  void read(uint64_t address, uint32_t length, Event eid, uint64_t data = 0);

  /**
   * \brief Write Memory
   *
   * Write Memory with callback event.
   *
   * \param[in] address Begin address of SRAM
   * \param[in] length  Amount of data to write
   * \param[in] eid     Event ID of callback event
   * \param[in] data    Event data
   */
  void write(uint64_t address, uint32_t length, Event eid, uint64_t data = 0);

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

  void getStatList(std::vector<Stat> &, std::string) noexcept;
  void getStatValues(std::vector<double> &) noexcept;
  void resetStatValues() noexcept;

  void createCheckpoint(std::ostream &) const noexcept;
  void restoreCheckpoint(std::istream &) noexcept;
};

}  // namespace SimpleSSD::Memory

#endif
