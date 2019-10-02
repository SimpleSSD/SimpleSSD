// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __MEM_SRAM_ABSTRACT_SRAM__
#define __MEM_SRAM_ABSTRACT_SRAM__

#include <cinttypes>

#include "util/abstract_fifo.hh"

namespace SimpleSSD::Memory::SRAM {

class AbstractSRAM : public AbstractFIFO {
 protected:
  struct Stats {
    uint64_t count;
    uint64_t size;

    Stats() { clear(); }

    void clear() {
      count = 0;
      size = 0;
    }
  };

  Config::SRAMStructure *pStructure;

  // TODO: Add SRAM power model (from McPAT?)

  // double totalEnergy;  // Unit: pJ
  // double totalPower;   // Unit: mW
  Stats readStat;
  Stats writeStat;

  void rangeCheck(uint64_t, uint64_t) noexcept;

 public:
  AbstractSRAM(ObjectData &, std::string);
  virtual ~AbstractSRAM();

  /**
   * \brief Read SRAM
   *
   * Read SRAM with callback event.
   *
   * \param[in] address Begin address of SRAM
   * \param[in] length  Amount of data to read
   * \param[in] eid     Event ID of callback event
   * \param[in] context User data of callback
   */
  virtual void read(uint64_t address, uint64_t length, Event eid,
                    void *context = nullptr) = 0;

  /**
   * \brief Write SRAM
   *
   * Write SRAM with callback event.
   *
   * \param[in] address Begin address of SRAM
   * \param[in] length  Amount of data to write
   * \param[in] eid     Event ID of callback event
   * \param[in] context User data of callback
   */
  virtual void write(uint64_t address, uint64_t length, Event eid,
                     void *context = nullptr) = 0;

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint() noexcept override;
  void restoreCheckpoint() noexcept override;
};

}  // namespace SimpleSSD::Memory::SRAM

#endif
