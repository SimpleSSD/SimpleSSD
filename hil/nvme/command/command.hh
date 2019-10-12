// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_HIL_NVME_COMMAND_COMMAND_HH__
#define __SIMPLESSD_HIL_NVME_COMMAND_COMMAND_HH__

// Admin commands
#include "hil/nvme/command/abort.hh"
#include "hil/nvme/command/async_event_request.hh"
#include "hil/nvme/command/create_cq.hh"
#include "hil/nvme/command/create_sq.hh"
#include "hil/nvme/command/delete_cq.hh"
#include "hil/nvme/command/delete_sq.hh"
#include "hil/nvme/command/format_nvm.hh"
#include "hil/nvme/command/get_feature.hh"
#include "hil/nvme/command/get_log_page.hh"
#include "hil/nvme/command/identify.hh"
#include "hil/nvme/command/namespace_attachment.hh"
#include "hil/nvme/command/namespace_management.hh"
#include "hil/nvme/command/set_feature.hh"

// NVM commands
#include "hil/nvme/command/compare.hh"
#include "hil/nvme/command/dataset_management.hh"
#include "hil/nvme/command/flush.hh"
#include "hil/nvme/command/read.hh"
#include "hil/nvme/command/write.hh"

namespace SimpleSSD::HIL::NVMe {

/**
 * \brief CommandData class
 *
 * Stores everything about current command.
 * You can find definition in abstract_command.cc file.
 */
class CommandData : public Object {
 protected:
  friend Command;

  // List of friends - All commands can access this elements.
  friend Abort;

  Command *parent;

  Controller *controller;
  Interface *interface;
  Arbitrator *arbitrator;
  InterruptManager *interrupt;
  DMAEngine *dmaEngine;

  SQContext *sqc;
  CQContext *cqc;

  CommandData(ObjectData &, Command *, ControllerData *);
  ~CommandData();

  void createResponse();

 public:
  uint64_t getGCID();
  CQContext *getResponse();

  Command *getParent();

  Arbitrator *getArbitrator();

  void getStatList(std::vector<Stat> &, std::string) noexcept override {}
  void getStatValues(std::vector<double> &) noexcept override {}
  void resetStatValues() noexcept override {}

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

class IOCommandData : public CommandData {
 protected:
  friend Command;

  // List of friends - All commands can access this elements.
  friend Flush;

  DMATag dmaTag;

  uint64_t slpn;
  uint64_t nlp;
  uint32_t skipFront;
  uint32_t skipEnd;

  uint64_t _slba;
  uint16_t _nlb;

  uint64_t size;
  uint8_t *buffer;

  uint64_t beginAt;  // For log

  IOCommandData(ObjectData &, Command *, ControllerData *);
  ~IOCommandData();

  void createDMAEngine(uint32_t, Event);
  void destroyDMAEngine();

 public:
  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

class CompareCommandData : public IOCommandData {
 protected:
  friend Command;

  // List of friends - All commands can access this elements.
  friend Compare;

  uint8_t complete;
  uint8_t *subBuffer;

  CompareCommandData(ObjectData &, Command *, ControllerData *);
  ~CompareCommandData();

 public:
  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::HIL::NVMe

#endif
