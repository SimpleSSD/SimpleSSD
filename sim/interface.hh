// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_SIM_INTERFACE_HH__
#define __SIMPLESSD_SIM_INTERFACE_HH__

#include "sim/engine.hh"

namespace SimpleSSD {

/**
 * \brief DMA interface object declaration
 *
 * Abstract class for DMA interface
 */
class DMAInterface {
 public:
  DMAInterface() {}
  virtual ~DMAInterface() {}

  /**
   * DMA read request function
   *
   * Simulator must read data to the buffer and call callback function with
   * provided context.
   *
   * \param[in]  offset   Address to read
   * \param[in]  length   # of bytes to read
   * \param[out] buffer   Buffer. can be nullptr
   * \param[in]  eid      Event ID of callback function
   * \param[in] eid       Corresponding data
   */
  virtual void read(uint64_t offset, uint64_t length, uint8_t *buffer,
                    Event eid, uint64_t data = 0) = 0;

  /**
   * DMA write request function
   *
   * Simulator must write data from the buffer and call callback function with
   * provided context.
   *
   * \param[in] offset    Address to write
   * \param[in] length    # of bytes to write
   * \param[in] buffer    Data to write. can be nullptr
   * \param[in] eid       Event ID of callback function
   * \param[in] eid       Corresponding data
   */
  virtual void write(uint64_t offset, uint64_t length, uint8_t *buffer,
                     Event eid, uint64_t data = 0) = 0;
};

/**
 * \brief Interface object declaration
 *
 * SimpleSSD Interface object. Simulator must provide APIs for accessing
 * host DRAM (DMA) and etcetera informations.
 */
class Interface : public DMAInterface {
 public:
  Interface() : DMAInterface() {}
  virtual ~Interface() {}

  /**
   * \brief Interrupt post function
   *
   * Simulator must send interrupt to corresponding interrupt controller
   * with provided interrupt vector.
   *
   * \param[in] iv  Interrupt vector
   * \param[in] set True when set the interrupt, False when clear the interrupt
   */
  virtual void postInterrupt(uint16_t iv, bool set) = 0;

  /**
   * \brief PCI/PCIe ID getter
   *
   * NVMe Identify Controller requires PCI VendorID and PCI Subsystem Vendor ID.
   *
   * \param[out] vid    PCI Vendor ID
   * \param[out] ssvid  PCI Subsystem Vendor ID
   */
  virtual void getPCIID(uint16_t &vid, uint16_t &ssvid) {
    vid = 0;
    ssvid = 0;
  }
};

}  // namespace SimpleSSD

#endif
