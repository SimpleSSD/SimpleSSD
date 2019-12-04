// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_HIL_NVME_SUBSYSTEM_HH__
#define __SIMPLESSD_HIL_NVME_SUBSYSTEM_HH__

#include <map>
#include <variant>

#include "hil/hil.hh"
#include "hil/nvme/command/command.hh"
#include "hil/nvme/def.hh"
#include "hil/nvme/namespace.hh"
#include "hil/nvme/queue_arbitrator.hh"
#include "sim/abstract_subsystem.hh"
#include "util/sorted_map.hh"

namespace SimpleSSD::HIL::NVMe {

class ControllerData;

class Subsystem : public AbstractSubsystem {
 protected:
  HIL *pHIL;

  ControllerID controllerID;

  std::map<ControllerID, ControllerData *> controllerList;
  std::map<uint32_t, Namespace *> namespaceList;
  std::map<ControllerID, std::set<uint32_t>> attachmentTable;

  HealthInfo health;

  uint32_t logicalPageSize;
  uint64_t totalLogicalPages;
  uint64_t allocatedLogicalPages;

  // Asynchronous Event Request
  std::vector<ControllerID> aenTo;

  // Admin Command objects
  DeleteSQ *commandDeleteSQ;
  CreateSQ *commandCreateSQ;
  GetLogPage *commandGetLogPage;
  DeleteCQ *commandDeleteCQ;
  CreateCQ *commandCreateCQ;
  Identify *commandIdentify;
  Abort *commandAbort;
  SetFeature *commandSetFeature;
  GetFeature *commandGetFeature;
  AsyncEventRequest *commandAsyncEventRequest;
  NamespaceManagement *commandNamespaceManagement;
  NamespaceAttachment *commandNamespaceAttachment;
  FormatNVM *commandFormatNVM;

  // NVM Command objects
  Flush *commandFlush;
  Write *commandWrite;
  Read *commandRead;
  Compare *commandCompare;
  DatasetManagement *commandDatasetManagement;

  // Command dispatcher
  bool dispatching;
  Event eventDispatch;

  void dispatch();

  bool _createNamespace(uint32_t, NamespaceInformation *);
  bool _destroyNamespace(uint32_t);

  bool submitCommand(ControllerData *, SQContext *);

 public:
  Subsystem(ObjectData &);
  virtual ~Subsystem();

  void shutdownCompleted(ControllerID);
  void complete(CommandTag);

  void triggerDispatch();

  void init() override;

  ControllerID createController(Interface *) noexcept override;
  AbstractController *getController(ControllerID) noexcept override;

  // Command interface
  HIL *getHIL() const;
  const std::map<uint32_t, Namespace *> &getNamespaceList() const;
  const std::set<uint32_t> *getAttachment(ControllerID ctrlid) const;
  const std::map<ControllerID, ControllerData *> &getControllerList() const;
  uint32_t getLPNSize() const;
  uint64_t getTotalPages() const;
  uint64_t getAllocatedPages() const;
  HealthInfo *getHealth(uint32_t);

  uint8_t attachNamespace(ControllerID, uint32_t, bool = true);
  uint8_t detachNamespace(ControllerID, uint32_t, bool = true);
  uint8_t createNamespace(NamespaceInformation *, uint32_t &);
  uint8_t destroyNamespace(uint32_t);

  void scheduleAEN(AsyncEventType, uint8_t, LogPageID);

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::HIL::NVMe

#endif
