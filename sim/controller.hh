// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIM_CONTROLLER_HH__
#define __SIM_CONTROLLER_HH__

#include "sim/interface.hh"
#include "sim/object.hh"

namespace SimpleSSD {

/**
 * \brief Abstract controller declaration
 *
 * All controllers - NVMe/SATA/UFS - inherits this class. So that SimpleSSD can
 * pass read/write register calls to correct controllers.
 */
class Controller : public Object {
 protected:
  Interface *pInterface;

 public:
  Controller(ObjectData &o, Interface *i) : Object(o), pInterface(i) {}
  Controller(const Controller &) = delete;
  Controller(Controller &&) noexcept = default;
  virtual ~Controller() {}

  Controller &operator=(const Controller &) = delete;
  Controller &operator=(Controller &&) noexcept = default;

  virtual uint64_t read(uint64_t, uint64_t, uint8_t *) noexcept = 0;
  virtual uint64_t write(uint64_t, uint64_t, uint8_t *) noexcept = 0;
};

}  // namespace SimpleSSD

#endif
