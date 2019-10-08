// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_SIM_ABSTRACT_CONTROLLER_HH__
#define __SIMPLESSD_SIM_ABSTRACT_CONTROLLER_HH__

#include "sim/abstract_subsystem.hh"
#include "sim/interface.hh"
#include "sim/object.hh"

namespace SimpleSSD {

/**
 * \brief Abstract controller declaration
 *
 * All controllers - NVMe/SATA/UFS - inherits this class. So that SimpleSSD can
 * pass read/write register calls to correct controllers.
 */
class AbstractController : public Object {
 protected:
  Interface *interface;          //!< Per-controller host interface
  AbstractSubsystem *subsystem;  //!< Connected subsystem

  ControllerID controllerID;

 public:
  AbstractController(ObjectData &o, ControllerID id, AbstractSubsystem *s,
                     Interface *i)
      : Object(o), interface(i), subsystem(s), controllerID(id) {}
  AbstractController(const AbstractController &) = delete;
  AbstractController(AbstractController &&) noexcept = default;
  virtual ~AbstractController() {}

  AbstractController &operator=(const AbstractController &) = delete;
  AbstractController &operator=(AbstractController &&) noexcept = default;

  ControllerID getControllerID() { return controllerID; }

  virtual uint64_t read(uint64_t, uint64_t, uint8_t *) noexcept = 0;
  virtual uint64_t write(uint64_t, uint64_t, uint8_t *) noexcept = 0;

  void createCheckpoint(std::ostream &out) const noexcept override {
    BACKUP_SCALAR(out, controllerID);
  }

  void restoreCheckpoint(std::istream &in) noexcept override {
    RESTORE_SCALAR(in, controllerID);
  }
};

}  // namespace SimpleSSD

#endif
