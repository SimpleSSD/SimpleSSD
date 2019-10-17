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

  CommandData(ObjectData &, Command *, ControllerData *);

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

  FRIEND_LIST

  DMATag dmaTag;

  uint64_t slpn;
  uint32_t nlp;
  uint32_t skipFront;
  uint32_t skipEnd;

  uint32_t nlp_done_hil;
  uint32_t nlp_done_dma;
  uint32_t lpnSize;

  uint64_t _slba;
  uint16_t _nlb;

  uint64_t size;
  uint8_t *buffer;

  uint64_t beginAt;  // For log

  IOCommandData(ObjectData &, Command *, ControllerData *);

  void createDMAEngine(uint32_t, Event);
  void destroyDMAEngine();

 public:
  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

class CompareCommandData : public IOCommandData {
 protected:
  friend Command;

  FRIEND_LIST

  uint8_t complete;
  uint8_t *subBuffer;

  CompareCommandData(ObjectData &, Command *, ControllerData *);

 public:
  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::HIL::NVMe

#endif
