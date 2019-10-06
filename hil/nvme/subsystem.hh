// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __HIL_NVME_SUBSYSTEM_HH__
#define __HIL_NVME_SUBSYSTEM_HH__

#include <map>
#include <variant>

#include "hil/hil.hh"
#include "hil/nvme/def.hh"
#include "hil/nvme/namespace.hh"
#include "sim/abstract_subsystem.hh"

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

  uint32_t logicalPageSize;
  uint64_t totalLogicalPages;
  uint64_t allocatedLogicalPages;

  bool createNamespace(uint32_t, Config::Disk *, NamespaceInformation *);
  bool destroyNamespace(uint32_t);

 public:
  Subsystem(ObjectData &);
  virtual ~Subsystem();

  void triggerDispatch(ControllerData &);

  void init() override;

  ControllerID createController(Interface *) noexcept override;
  AbstractController *getController(ControllerID) noexcept override;

  // Command interface
  const HILPointer &getHIL() const;
  const std::map<uint32_t, Namespace *> &getNamespaceList() const;
  const std::set<uint32_t> *getAttachment(ControllerID ctrlid) const;
  const std::map<ControllerID, ControllerData *> &getControllerList() const;
  const uint32_t getLPNSize() const;
  const uint64_t getTotalPages() const;
  const uint64_t getAllocatedPages() const;

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::HIL::NVMe

#endif
