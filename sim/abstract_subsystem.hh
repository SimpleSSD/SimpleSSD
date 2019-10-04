// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIM_ABSTRACT_SUBSYSTEM_HH__
#define __SIM_ABSTRACT_SUBSYSTEM_HH__

#include <map>
#include <set>

#include "sim/interface.hh"
#include "sim/object.hh"

namespace SimpleSSD {

// Forward declaration
class AbstractController;
class AbstractSubsystem;

/**
 * \brief Controller ID definition
 *
 * As only NVMe defines multiple-controller concept, just follows NVMe's
 * controller ID size = 2Byte.
 */
using ControllerID = uint16_t;

//! Controller data
class ControllerData {
 private:
  friend AbstractSubsystem;

  ControllerID id;
  std::set<uint32_t> attachedNamespaceList;

 public:
  AbstractController *controller;
  Interface *interface;     //!< Top-most host interface
  DMAInterface *dma;        //!< DMA port for current controller
  uint64_t memoryPageSize;  //!< This is only for PRPEngine

  ControllerData();
  ControllerData(AbstractController *, Interface *, DMAInterface *, uint64_t);
};

/**
 * \brief Abstract subsystem declaration
 *
 * NVM Subsystem. Although only NVMe defines NVM Subsystem, we just use this
 * class for define a collection of NVM media. This is because to support
 * multiple controllers in one NVM subsystem in NVMe. All SSD inherits this
 * class and return SimpleSSD::AbstractController object when
 * SimpleSSD::AbstractSubsystem::getController called with controller ID.
 * For SSD interfaces other than NVMe, only one controller is supported
 * (controller ID = 1).
 */
class AbstractSubsystem : public Object {
 protected:
  std::map<ControllerID, ControllerData> controllerList;

 public:
  AbstractSubsystem(ObjectData &);
  AbstractSubsystem(const AbstractSubsystem &) = delete;
  AbstractSubsystem(AbstractSubsystem &&) noexcept = default;
  virtual ~AbstractSubsystem();

  AbstractSubsystem &operator=(const AbstractSubsystem &) = delete;
  AbstractSubsystem &operator=(AbstractSubsystem &&) noexcept = default;

  virtual ControllerID createController(Interface *) noexcept = 0;
  virtual void destroyController(ControllerID) noexcept = 0;

  virtual AbstractController *getController(ControllerID) noexcept;
};

}  // namespace SimpleSSD

#endif
