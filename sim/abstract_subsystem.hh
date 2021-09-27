// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_SIM_ABSTRACT_SUBSYSTEM_HH__
#define __SIMPLESSD_SIM_ABSTRACT_SUBSYSTEM_HH__

#include <map>
#include <set>

#include "hil/request.hh"
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
  virtual ~AbstractSubsystem() {}

  virtual void init() = 0;

  virtual ControllerID createController(Interface *) noexcept = 0;
  virtual AbstractController *getController(ControllerID) noexcept = 0;

  /**
   * \brief Get internal I/O queue status
   *
   * This function used for implementing advanced garbage collection algorithm
   * which uses request idle-time to schedule GC operation. Return two values:
   * one for # of not-handled, pending I/O requests, other for # of in-progress
   * I/O requests.
   *
   * \param[out] waitingRequests  # of I/O requests in waiting
   * \param[out] handlingRequests # of I/O requests in handling
   */
  virtual void getQueueStatus(uint64_t &waitingRequests,
                              uint64_t &handlingRequests) noexcept = 0;

  virtual HIL::Request *restoreRequest(uint64_t) noexcept = 0;
};

}  // namespace SimpleSSD

#endif
