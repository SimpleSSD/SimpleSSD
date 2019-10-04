// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIM_ABSTRACT_CONTROLLER_HH__
#define __SIM_ABSTRACT_CONTROLLER_HH__

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

  ControllerData controllerData;

 public:
  AbstractController(ObjectData &o, Interface *i, AbstractSubsystem *s)
      : Object(o), interface(i), subsystem(s) {}
  AbstractController(const AbstractController &) = delete;
  AbstractController(AbstractController &&) noexcept = default;
  virtual ~AbstractController() {}

  AbstractController &operator=(const AbstractController &) = delete;
  AbstractController &operator=(AbstractController &&) noexcept = default;

  virtual ControllerData &getControllerData() noexcept {
    return controllerData;
  }

  virtual uint64_t read(uint64_t, uint64_t, uint8_t *) noexcept = 0;
  virtual uint64_t write(uint64_t, uint64_t, uint8_t *) noexcept = 0;

  void createCheckpoint(std::ostream &out) noexcept override {
    BACKUP_SCALAR(out, controllerData.memoryPageSize);
  }

  void restoreCheckpoint(std::istream &in) noexcept override {
    RESTORE_SCALAR(in, controllerData.memoryPageSize);
  }
};

}  // namespace SimpleSSD

#endif
