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
#include "hil/nvme/command/async_event_request.hh"
#include "hil/nvme/def.hh"
#include "hil/nvme/namespace.hh"
#include "hil/nvme/queue_arbitrator.hh"
#include "sim/abstract_subsystem.hh"
#include "util/sorted_map.hh"

namespace SimpleSSD::HIL::NVMe {

class ControllerData;

class Subsystem : public AbstractSubsystem {
 protected:
  using HILPointer =
      std::variant<HIL<uint16_t> *, HIL<uint32_t> *, HIL<uint64_t> *>;

  HILPointer pHIL;

  ControllerID controllerID;

  std::map<ControllerID, ControllerData *> controllerList;
  std::map<uint32_t, Namespace *> namespaceList;
  std::map<ControllerID, std::set<uint32_t>> attachmentTable;

  HealthInfo health;

  unordered_map_queue ongoingCommands;

  Event eventAEN;
  std::vector<ControllerID> aenTo;
  uint32_t aenData;

  unordered_map_queue aenCommands;

  uint32_t logicalPageSize;
  uint64_t totalLogicalPages;
  uint64_t allocatedLogicalPages;

  bool _createNamespace(uint32_t, Config::Disk *, NamespaceInformation *);
  bool _destroyNamespace(uint32_t);

  Command *makeCommand(ControllerData *, SQContext *);
  void invokeAEN();
  void scheduleAEN(AsyncEventType, uint8_t, LogPageID);

 public:
  Subsystem(ObjectData &);
  virtual ~Subsystem();

  void triggerDispatch(ControllerData &, uint64_t);
  void complete(Command *, bool = false);

  void init() override;

  ControllerID createController(Interface *) noexcept override;
  AbstractController *getController(ControllerID) noexcept override;

  // Command interface
  const HILPointer &getHIL() const;
  const std::map<uint32_t, Namespace *> &getNamespaceList() const;
  const std::set<uint32_t> *getAttachment(ControllerID ctrlid) const;
  const std::map<ControllerID, ControllerData *> &getControllerList() const;
  uint32_t getLPNSize() const;
  uint64_t getTotalPages() const;
  uint64_t getAllocatedPages() const;
  HealthInfo *getHealth(uint32_t);

  uint8_t attachController(ControllerID, uint32_t, bool = true);
  uint8_t detachController(ControllerID, uint32_t, bool = true);
  uint8_t createNamespace(NamespaceInformation *, uint32_t &);
  uint8_t destroyNamespace(uint32_t);

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::HIL::NVMe

#endif
