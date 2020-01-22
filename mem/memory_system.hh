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

class System {
 private:
  CPU::CPU *cpu;
  ConfigReader *config;
  Log *log;

  struct MemoryMap {
    std::string name;
    uint64_t base;
    uint64_t size;

    MemoryMap(std::string &&n, uint64_t b, uint64_t s)
        : name(std::move(n)), base(b), size(s) {}
  };

  uint64_t totalCapacity;
  std::vector<MemoryMap> addressMap;

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
   * \param[in] name  Description of memory range
   * \param[in] dry   Dry-run (to check memory is allocatable)
   * \return address  Beginning address of allocated range
   */
  uint64_t allocate(uint64_t size, std::string &&name, bool dry = false);

  void getStatList(std::vector<Stat> &, std::string) noexcept;
  void getStatValues(std::vector<double> &) noexcept;
  void resetStatValues() noexcept;

  void createCheckpoint(std::ostream &) const noexcept;
  void restoreCheckpoint(std::istream &) noexcept;
};

}  // namespace SimpleSSD::Memory

#endif
