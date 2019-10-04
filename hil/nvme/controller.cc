// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "hil/nvme/controller.hh"

namespace SimpleSSD::HIL::NVMe {

Controller::RegisterTable::RegisterTable() {
  memset(data, 0, sizeof(RegisterTable));
}

Controller::PersistentMemoryRegion::PersistentMemoryRegion() {
  memset(data, 0, sizeof(PersistentMemoryRegion));
}

Controller::Controller(ObjectData &o, Interface *i, AbstractSubsystem *p)
    : AbstractController(o, i, p),
      interruptManager(o, i),
      arbitrator(o, controllerData),
      adminQueueInited(0),
      interruptMask(0),
      adminQueueCreated(0) {
  // Initialize FIFO queues
  auto axiWidth = (ARM::AXI::Width)readConfigUint(Section::HostInterface,
                                                  Config::Key::AXIWidth);
  auto axiClock = readConfigUint(Section::HostInterface, Config::Key::AXIClock);

  FIFOParam param;

  param.rqSize = 8192;
  param.wqSize = 8192;
  param.transferUnit = 64;
  param.latency = ARM::AXI::makeFunction(axiClock, axiWidth);

  pcie = new FIFO(o, i, param);

  // [Bits ] Name  : Description                        : Current Setting
  // [63:58] Reserved
  // [57:57] CMBS  : Controller Memory Buffer Supported : No
  // [56:56] PMRS  : Persistent Memory Region Supported : No
  // [55:52] MPSMZX: Memory Page Size Maximum           : 2^14 Bytes
  // [51:48] MPSMIN: Memory Page Size Minimum           : 2^12 Bytes
  // [47:46] Reserved
  // [45:45] BPS   : Boot Partition Supported           : No
  // [44:37] CSS   : Command Sets Supported             : NVM command set
  // [36:36] NSSRS : NVM Subsystem Reset Supported      : No
  // [35:32] DSTRD : Doorbell Stride                    : 0 (4 bytes)
  registers.controllerCapabilities =
      0b000000'0'0'0010'0000'00'0'00000001'0'0000ull << 32;

  // [Bits ] Name  : Description                        : Current Setting
  // [31:24] TO    : Timeout                            : 40 * 500ms
  // [23:19] Reserved
  // [18:17] AMS   : Arbitration Mechanism Supported    : Weighted Round Robin
  // [16:16] CQR   : Contiguous Queues Required         : No
  registers.controllerCapabilities |= 0b00101000'00000'01'0ull << 16;

  // [15:00] MQES  : Maximum Queue Entries Supported    : 65536 Entries
  registers.controllerCapabilities |= (uint16_t)65535;

  registers.version = 0x00010400;  // NVMe 1.4

  // Create events
  eventQueueInit = createEvent(
      [this](uint64_t, EventContext) {
        adminQueueCreated++;

        if (adminQueueCreated == 2 && registers.cc.en &&
            registers.cs.rdy == 0) {
          registers.cs.rdy = 1;

          arbitrator.enable(true);
        }
      },
      "HIL::NVMe::Controller::eventQueueInit");
  eventInterrupt = createEvent(
      [this](uint64_t, EventContext c) {
        postInterrupt(c.get<InterruptContext *>());
      },
      "HIL::NVMe::Controller::eventInterrupt");
  eventSubmit =
      createEvent([this](uint64_t, EventContext) { notifySubsystem(); },
                  "HIL::NVMe::Controller::eventSubmit");
  eventShutdown = createEvent(
      [this](uint64_t, EventContext) {
        registers.cs.rdy = 0;   // RDY = 0
        registers.cs.shst = 2;  // Shutdown processing complete
      },
      "HIL::NVMe::Controller::eventShutdown");

  eventComplete = arbitrator.init(eventSubmit, eventInterrupt, eventShutdown);
}

Controller::~Controller() {
  delete pcie;
  // delete interconnect;
}

void Controller::postInterrupt(InterruptContext *context) {
  interruptManager.postInterrupt(context->iv, context->post);
}

void Controller::notifySubsystem() {
  // TODO: FILL HERE
}

uint64_t Controller::read(uint64_t offset, uint64_t size,
                          uint8_t *buffer) noexcept {
  registers.interruptMaskSet = interruptMask;
  registers.interruptMaskClear = interruptMask;

  memcpy(buffer, registers.data + offset, size);

  // TODO: add debug print

  return 0;
}

uint64_t Controller::write(uint64_t offset, uint64_t size,
                           uint8_t *buffer) noexcept {
  uint32_t uiTemp32;
  uint64_t uiTemp64;

  if (size == 4) {
    memcpy(&uiTemp32, buffer, 4);

    switch ((Register)offset) {
      case Register::InterruptMaskSet:
        interruptMask |= uiTemp32;

        break;
      case Register::InterruptMaskClear:
        interruptMask &= ~uiTemp32;

        break;
      case Register::ControllerConfiguration:
        handleControllerConfig(uiTemp32);

        break;
      case Register::ControllerStatus:
        // Clear NSSRO if set
        if (uiTemp32 & 0x00000010) {
          registers.controllerStatus &= 0xFFFFFFEF;
        }

        break;
      case Register::NVMSubsystemReset:
        registers.subsystemReset = uiTemp32;

        // FIXME: If NSSR is same as NVMe(0x4E564D65), do NVMe Subsystem reset
        // (when CAP.NSSRS is 1)
        break;
      case Register::AdminQueueAttributes:
        registers.adminQueueAttributes &= 0xF000F000;
        registers.adminQueueAttributes |= (uiTemp32 & 0x0FFF0FFF);

        break;
      case Register::AdminCQBaseAddress:
        memcpy(&(registers.adminCQueueBaseAddress), buffer, 4);
        adminQueueInited++;

        break;
      case Register::AdminCQBaseAddressH:
        memcpy(((uint8_t *)&(registers.adminCQueueBaseAddress)) + 4, buffer, 4);
        adminQueueInited++;

        break;
      case Register::AdminSQBaseAddress:
        memcpy(&(registers.adminSQueueBaseAddress), buffer, 4);
        adminQueueInited++;

        break;
      case Register::AdminSQBaseAddressH:
        memcpy(((uint8_t *)&(registers.adminSQueueBaseAddress)) + 4, buffer, 4);
        adminQueueInited++;

        break;
      default:
        if (offset < (uint64_t)Register::DoorbellBegin) {
          panic("Write on read only register");
        }

        break;
    }

    if (offset >= (uint64_t)Register::DoorbellBegin) {
      const int dstrd = 4;
      uint32_t uiTemp, uiMask;
      uint16_t uiDoorbell;

      // Get new doorbell
      memcpy(&uiDoorbell, buffer, 2);

      // Calculate Queue Type and Queue ID from offset
      offset -= (uint64_t)Register::DoorbellBegin;
      uiTemp = offset / dstrd;
      uiMask = (uiTemp & 0x00000001);  // 0 for Submission Queue Tail Doorbell
                                       // 1 for Completion Queue Head Doorbell
      uiTemp = (uiTemp >> 1);          // Queue ID

      if (uiMask) {  // Completion Queue
        arbitrator.ringCQ(uiTemp, uiDoorbell);
      }
      else {  // Submission Queue
        arbitrator.ringSQ(uiTemp, uiDoorbell);
      }
    }
  }
  else if (size == 8) {
    memcpy(&uiTemp64, buffer, 8);

    switch ((Register)offset) {
      case Register::AdminCQBaseAddress:
        registers.adminCQueueBaseAddress = uiTemp64;
        adminQueueInited += 2;

        break;
      case Register::AdminSQBaseAddress:
        registers.adminSQueueBaseAddress = uiTemp64;
        adminQueueInited += 2;

        break;
      default:
        panic("Write on read only register");

        break;
    }
  }
  else {
    panic("Invalid read size (%d) on controller register", size);
  }

  if (adminQueueInited == 4) {
    uint16_t entrySize = 0;

    adminQueueInited = 0;

    entrySize = ((registers.adminQueueAttributes & 0x0FFF0000) >> 16) + 1;
    arbitrator.createAdminCQ(registers.adminCQueueBaseAddress, entrySize,
                             eventQueueInit, EventContext());

    entrySize = (registers.adminQueueAttributes & 0x0FFF) + 1;
    arbitrator.createAdminSQ(registers.adminSQueueBaseAddress, entrySize,
                             eventQueueInit, EventContext());
  }

  return 0;
}

void Controller::handleControllerConfig(uint32_t update) {
  auto old = registers.controllerConfiguration;

  // Write new value
  registers.controllerConfiguration &= 0xFF00000E;
  registers.controllerConfiguration |= (update & 0x00FFFFF1);

  // Check which field is updated
  if (((old >> 20) & 0x0F) != registers.cc.iocqes) {
    // I/O Completion Queue Entry Size changed
    cqStride = 1ull << registers.cc.iocqes;
  }
  if (((old >> 16) & 0x0F) != registers.cc.iosqes) {
    // I/O Submission Queue Entry Size changed
    sqStride = 1ull << registers.cc.iosqes;
  }
  if (((old >> 14) & 0x03) != registers.cc.shn) {
    // Shutdown Notification changed
    if (update & 0x0000C000) {
      registers.cs.rdy = 1;   // RDY = 1
      registers.cs.shst = 1;  // Shutdown processing occurring

      arbitrator.reserveShutdown();
    }
  }
  if (((old >> 11) & 0x07) != registers.cc.ams) {
    // Arbitration Mechanism Selected changed
    arbitration = (Arbitration)registers.cc.ams;
  }
  if (((old >> 7) & 0x0F) != registers.cc.mps) {
    // Memory Page Size changed
    controllerData.memoryPageSize = 1ull << (registers.cc.mps + 12);
  }
  if (((old >> 4) & 0x07) != registers.cc.css) {
    // I/O Command Set Selected changed
    warn_if(registers.cc.css != 0, "Unexpedted I/O command set.");
  }
  if ((old & 0x01) != registers.cc.en) {
    // Enable changed
    if (registers.cc.en) {
      if (adminQueueCreated == 2) {
        registers.cs.rdy = 1;

        arbitrator.enable(true);
      }
    }
    else {
      registers.cs.rdy = 0;

      arbitrator.enable(false);
    }
  }
}

void Controller::getStatList(std::vector<Stat> &, std::string) noexcept {}

void Controller::getStatValues(std::vector<double> &) noexcept {}

void Controller::resetStatValues() noexcept {}

void Controller::createCheckpoint(std::ostream &out) noexcept {
  AbstractController::createCheckpoint(out);

  interruptManager.createCheckpoint(out);
  arbitrator.createCheckpoint(out);
  pcie->createCheckpoint(out);

  BACKUP_BLOB(out, registers.data, sizeof(RegisterTable));
  BACKUP_BLOB(out, pmrRegisters.data, sizeof(PersistentMemoryRegion));
  BACKUP_SCALAR(out, sqStride);
  BACKUP_SCALAR(out, cqStride);
  BACKUP_SCALAR(out, adminQueueInited);
  BACKUP_SCALAR(out, arbitration);
  BACKUP_SCALAR(out, interruptMask);
  BACKUP_SCALAR(out, adminQueueCreated);
}

void Controller::restoreCheckpoint(std::istream &in) noexcept {
  AbstractController::restoreCheckpoint(in);

  interruptManager.restoreCheckpoint(in);
  arbitrator.restoreCheckpoint(in);
  pcie->restoreCheckpoint(in);

  RESTORE_BLOB(in, registers.data, sizeof(RegisterTable));
  RESTORE_BLOB(in, pmrRegisters.data, sizeof(PersistentMemoryRegion));
  RESTORE_SCALAR(in, sqStride);
  RESTORE_SCALAR(in, cqStride);
  RESTORE_SCALAR(in, adminQueueInited);
  RESTORE_SCALAR(in, arbitration);
  RESTORE_SCALAR(in, interruptMask);
  RESTORE_SCALAR(in, adminQueueCreated);
}

}  // namespace SimpleSSD::HIL::NVMe
