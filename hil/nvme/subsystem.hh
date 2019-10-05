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

class Subsystem : public AbstractSubsystem {
 protected:
  using HILPointer =
      std::variant<HIL<uint16_t> *, HIL<uint32_t> *, HIL<uint64_t> *>;

  HILPointer pHIL;

  ControllerID controllerID;

  std::map<uint32_t, Namespace *> namespaceList;

  uint32_t logicalPageSize;
  uint64_t totalLogicalPages;
  uint64_t allocatedLogicalPages;

  bool createNamespace(uint32_t, Config::Disk *, NamespaceInformation *);
  bool destroyNamespace(uint32_t);

 public:
  Subsystem(ObjectData &);
  virtual ~Subsystem();

  void init() override;

  ControllerID createController(Interface *) noexcept override;

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::HIL::NVMe

#endif
