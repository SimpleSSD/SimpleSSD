// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_HIL_NVME_COMMAND_COMMAND_HH__
#define __SIMPLESSD_HIL_NVME_COMMAND_COMMAND_HH__

#include "hil/request.hh"

// Admin commands
#include "hil/nvme/command/admin/abort.hh"
#include "hil/nvme/command/admin/async_event_request.hh"
#include "hil/nvme/command/admin/create_cq.hh"
#include "hil/nvme/command/admin/create_sq.hh"
#include "hil/nvme/command/admin/delete_cq.hh"
#include "hil/nvme/command/admin/delete_sq.hh"
#include "hil/nvme/command/admin/get_feature.hh"
#include "hil/nvme/command/admin/get_log_page.hh"
#include "hil/nvme/command/admin/identify.hh"
#include "hil/nvme/command/admin/namespace_attachment.hh"
#include "hil/nvme/command/admin/namespace_management.hh"
#include "hil/nvme/command/admin/set_feature.hh"

// NVM command set specific
#include "hil/nvme/command/nvm/compare.hh"
#include "hil/nvme/command/nvm/dataset_management.hh"
#include "hil/nvme/command/nvm/flush.hh"
#include "hil/nvme/command/nvm/format_nvm.hh"
#include "hil/nvme/command/nvm/read.hh"
#include "hil/nvme/command/nvm/write.hh"

namespace SimpleSSD::HIL::NVMe {

#define FRIEND_LIST                                                            \
  friend DeleteSQ;                                                             \
  friend CreateSQ;                                                             \
  friend GetLogPage;                                                           \
  friend DeleteCQ;                                                             \
  friend CreateCQ;                                                             \
  friend Identify;                                                             \
  friend Abort;                                                                \
  friend SetFeature;                                                           \
  friend GetFeature;                                                           \
  friend AsyncEventRequest;                                                    \
  friend NamespaceManagement;                                                  \
  friend NamespaceAttachment;                                                  \
  friend FormatNVM;                                                            \
  friend Flush;                                                                \
  friend Write;                                                                \
  friend Read;                                                                 \
  friend Compare;                                                              \
  friend DatasetManagement;

/**
 * \brief CommandData class
 *
 * Stores everything about current command.
 * You can find definition in abstract_command.cc file.
 */
class CommandData : public Object {
 protected:
  friend Command;

  FRIEND_LIST

  Command *parent;

  Controller *controller;
  Interface *interface;
  Arbitrator *arbitrator;
  InterruptManager *interrupt;
  DMAEngine *dmaEngine;

  SQContext *sqc;
  CQContext *cqc;

  Request request;
  uint64_t beginAt;

  std::vector<uint8_t> buffer;

  CommandData(ObjectData &, Command *, ControllerData *);

  void createResponse();

 public:
  uint64_t getGCID();
  CQContext *getResponse();

  Command *getParent();

  Arbitrator *getArbitrator();

  void initRequest(Event);
  void makeResponse();
  void createDMAEngine(uint32_t, Event);
  void destroyDMAEngine();

  void getStatList(std::vector<Stat> &, std::string) noexcept override {}
  void getStatValues(std::vector<double> &) noexcept override {}
  void resetStatValues() noexcept override {}

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::HIL::NVMe

#endif
