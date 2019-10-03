// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __HIL_NVME_CONTROLLER_HH__
#define __HIL_NVME_CONTROLLER_HH__

#include "hil/common/interrupt_manager.hh"
#include "hil/nvme/queue_arbitrator.hh"
#include "sim/abstract_controller.hh"
#include "util/fifo.hh"

namespace SimpleSSD::HIL::NVMe {

/**
 * \brief NVMe Controller object
 *
 * Declaration of NVMe Controller, which mapped to one PCIe function.
 * Contains controller registers, queue arbitrator and DMA engine.
 */
class Controller : public AbstractController {
 private:
  union RegisterTable {
    uint8_t data[0x5C];
    struct {
      uint64_t controllerCapabilities;
      uint32_t version;
      uint32_t interruptMaskSet;
      uint32_t interruptMaskClear;
      uint32_t controllerConfiguration;
      uint32_t reserved;
      uint32_t controllerStatus;
      uint32_t subsystemReset;
      uint32_t adminQueueAttributes;
      uint64_t adminSQueueBaseAddress;
      uint64_t adminCQueueBaseAddress;
      uint32_t controllerMemoryBufferLocation;
      uint32_t controllerMemoryBufferSize;
      uint32_t bootPartitionInformation;
      uint32_t bootPartitionReadSelect;
      uint64_t bootPartitionMemorybufferLocation;
      uint64_t controllerMemoryBufferMemoryspaceControl;
      uint32_t controllerMemoryBufferStatus;
    };

    RegisterTable();
  };

  union PersistentMemoryRegion {
    uint8_t data[0x1C];
    struct {
      uint32_t capabilities;
      uint32_t control;
      uint32_t status;
      uint32_t elasticityBufferSize;
      uint32_t sustainedWriteThroughput;
      uint64_t controllerMemorySpaceControl;
    };

    PersistentMemoryRegion();
  };

  // TODO: Add Subsystem HERE!
  InterruptManager interruptManager;
  Arbitrator arbitrator;

  FIFO *pcie;          //!< FIFO for PCIe bus <-> PCIe PHY
  // FIFO *interconnect;  //!< FIFO for PCIe PHY <-> Interconnect

  RegisterTable registers;
  PersistentMemoryRegion pmrRegisters;

  uint64_t sqStride;         //!< Calculated SQ stride
  uint64_t cqStride;         //!< Calculated CQ stride
  uint8_t adminQueueInited;  //!< uint8_t, not bool, for checkout pointer counts
  Arbitration arbitration;   //!< Selected arbitration mechanism
  uint32_t interruptMask;    //!< Current interrupt mask

  // TODO: May be removed
  bool shutdownReserved;

  // Arbitrator -> InterruptManager
  Event eventInterrupt;
  void postInterrupt(InterruptContext *);

  // Arbitrator -> Subsystem
  Event eventSubmit;
  void notifySubsystem();

 public:
  Controller(ObjectData &, Interface *);
  ~Controller();

  uint64_t read(uint64_t, uint64_t, uint8_t *) noexcept override;
  uint64_t write(uint64_t, uint64_t, uint8_t *) noexcept override;
};

}  // namespace SimpleSSD::HIL::NVMe

#endif
