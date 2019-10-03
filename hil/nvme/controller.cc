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

Controller::Controller(ObjectData &o, Interface *i)
    : AbstractController(o, i),
      interruptManager(o, i),
      arbitrator(o),
      adminQueueInited(false),
      interruptMask(0),
      shutdownReserved(false) {
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

  // TODO: Create Subsystem

  // Create events
  eventInterrupt = createEvent(
      [this](uint64_t, void *c) { postInterrupt((InterruptContext *)c); },
      "HIL::NVMe::Controller::eventInterrupt");
  eventSubmit = createEvent([this](uint64_t, void *) { notifySubsystem(); },
                            "HIL::NVMe::Controller::eventSubmit");
}

Controller::~Controller() {
  delete pcie;
}

}  // namespace SimpleSSD::HIL::NVMe
