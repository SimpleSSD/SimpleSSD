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

class Subsystem;

class ControllerData {
 public:
  Controller *controller;
  Interface *interface;     //!< Top-most host interface
  DMAInterface *dma;        //!< DMA port for current controller
  InterruptManager *interruptManager;
  Arbitrator *arbitrator;
  uint64_t memoryPageSize;  //!< This is only for PRPEngine

  ControllerData();
  ControllerData(Controller *, Interface *, DMAInterface *, uint64_t);
};

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
      union {
        uint32_t controllerConfiguration;
        struct {
          uint32_t en : 1;
          uint32_t rsvd1 : 3;
          uint32_t css : 3;
          uint32_t mps : 4;
          uint32_t ams : 3;
          uint32_t shn : 2;
          uint32_t iosqes : 4;
          uint32_t iocqes : 4;
          uint32_t rsvd2 : 8;
        } cc;
      };
      uint32_t reserved;
      union {
        uint32_t controllerStatus;
        struct {
          uint32_t rdy : 1;
          uint32_t cfs : 1;
          uint32_t shst : 2;
          uint32_t nssro : 1;
          uint32_t pp : 1;
          uint32_t rsvd : 26;
        } cs;
      };
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

  ControllerData controllerData;

  // FIFO *pcie;  //!< FIFO for PCIe bus <-> PCIe PHY
  // FIFO *interconnect;  //!< FIFO for PCIe PHY <-> Interconnect

  RegisterTable registers;
  PersistentMemoryRegion pmrRegisters;

  uint64_t sqStride;         //!< Calculated I/O SQ stride
  uint64_t cqStride;         //!< Calculated I/O CQ stride
  uint8_t adminQueueInited;  //!< uint8_t, not bool, for checkout pointer counts
  Arbitration arbitration;   //!< Selected arbitration mechanism
  uint32_t interruptMask;    //!< Current interrupt mask

  // Queue DMA initialization
  uint8_t adminQueueCreated;  //!< When queue DMAEngine initialized

  Event eventQueueInit;

  void handleControllerConfig(uint32_t);

 public:
  Controller(ObjectData &, ControllerID, Subsystem *, Interface *);
  ~Controller();

  // Arbitrator
  void notifySubsystem();
  void shutdownComplete();

  ControllerData &getControllerData();

  uint64_t read(uint64_t, uint64_t, uint8_t *) noexcept override;
  uint64_t write(uint64_t, uint64_t, uint8_t *) noexcept override;

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::HIL::NVMe

#endif
