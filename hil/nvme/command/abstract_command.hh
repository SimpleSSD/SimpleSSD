// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __HIL_NVME_COMMAND_ABSTRACT_COMMAND_HH__
#define __HIL_NVME_COMMAND_ABSTRACT_COMMAND_HH__

#include <queue>

#include "hil/nvme/dma_engine.hh"
#include "sim/interface.hh"
#include "sim/object.hh"

namespace SimpleSSD::HIL::NVMe {

class Subsystem;
class Controller;
class Arbitrator;
class InterruptManager;
class SQContext;
class CQContext;
class ControllerData;

class CommandData {
 public:
  Subsystem *subsystem;
  Controller *controller;
  Interface *interface;
  DMAInterface *dma;
  Arbitrator *arbitrator;
  InterruptManager *interrupt;
  uint64_t memoryPageSize;

  CommandData(Subsystem *, ControllerData *);
};

/**
 * \brief Command object
 *
 * All NVMe command inherits this object.
 */
class Command : public Object {
 protected:
  CommandData data;

  DMAEngine *dmaEngine;

  SQContext *sqc;
  CQContext *cqc;

  void createResponse();
  void createDMAEngine(uint64_t, Event);

 public:
  Command(ObjectData &, Subsystem *, ControllerData *);
  Command(const Command &) = delete;
  Command(Command &&) noexcept = default;
  virtual ~Command();

  CQContext *getResult();
  CommandData &getCommandData();
  uint64_t getUniqueID();
  virtual void setRequest(SQContext *) = 0;

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::HIL::NVMe

#endif
