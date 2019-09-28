// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __MEM_DRAM_ABSTRACT_DRAM__
#define __MEM_DRAM_ABSTRACT_DRAM__

#include <cinttypes>

#include "libdrampower/LibDRAMPower.h"
#include "sim/object.hh"

namespace SimpleSSD::Memory::DRAM {

typedef enum : uint8_t {
  ACTIVE,                //!< Row active
  IDLE,                  //!< Precharged
  POWER_DOWN_PRECHARGE,  //!< Precharge power down
  POWER_DOWN_ACTIVE,     //!< Active power down
  SELF_REFRESH,          //!< Self refresh
} DRAMState;

class AbstractDRAM : public Object {
 protected:
  Config::DRAMStructure *pStructure;
  Config::DRAMTiming *pTiming;
  Config::DRAMPower *pPower;

  Data::MemorySpecification spec;
  libDRAMPower *dramPower;

  void convertMemspec();

  double totalEnergy;  // Unit: pJ
  double totalPower;   // Unit: mW

 public:
  AbstractDRAM(ObjectData &);
  virtual ~AbstractDRAM();

  /**
   * \brief Read DRAM
   *
   * Read DRAM with callback event.
   *
   * \param[in] address Begin address of DRAM
   * \param[in] length  Amount of data to read
   * \param[in] eid     Event ID of callback event
   * \param[in] context User data of callback
   */
  virtual void read(uint64_t address, uint64_t length, Event eid,
                    void *context = nullptr) = 0;

  /**
   * \brief Write DRAM
   *
   * Write DRAM with callback event.
   *
   * \param[in] address Begin address of DRAM
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

}  // namespace SimpleSSD::Memory::DRAM

#endif
