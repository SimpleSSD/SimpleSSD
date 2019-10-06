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

/**
 * \brief Controller ID definition
 *
 * As only NVMe defines multiple-controller concept, just follows NVMe's
 * controller ID size = 2Byte.
 */
using ControllerID = uint16_t;

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
  bool inited;

 public:
  AbstractSubsystem(ObjectData &o) : Object(o), inited(false) {}
  AbstractSubsystem(const AbstractSubsystem &) = delete;
  AbstractSubsystem(AbstractSubsystem &&) noexcept = default;
  virtual ~AbstractSubsystem() {}

  AbstractSubsystem &operator=(const AbstractSubsystem &) = delete;
  AbstractSubsystem &operator=(AbstractSubsystem &&) noexcept = default;

  virtual void init() = 0;

  virtual ControllerID createController(Interface *) noexcept = 0;
  virtual AbstractController *getController(ControllerID) noexcept = 0;
};

}  // namespace SimpleSSD

#endif
