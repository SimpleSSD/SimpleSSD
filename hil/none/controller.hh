// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_HIL_NONE_CONTROLLER_HH__
#define __SIMPLESSD_HIL_NONE_CONTROLLER_HH__

#include "sim/abstract_controller.hh"

namespace SimpleSSD::HIL::None {

class Subsystem;

class Controller : public AbstractController {
 public:
  Controller(ObjectData &, ControllerID, Subsystem *, Interface *);
  ~Controller();

  HIL *getHIL() const;

  uint64_t read(uint64_t, uint32_t, uint8_t *) noexcept override;
  uint64_t write(uint64_t, uint32_t, uint8_t *) noexcept override;

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::HIL::None

#endif
