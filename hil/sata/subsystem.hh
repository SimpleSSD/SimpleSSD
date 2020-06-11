// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_HIL_SATA_SUBSYSTEM_HH__
#define __SIMPLESSD_HIL_SATA_SUBSYSTEM_HH__

#include "hil/hil.hh"
#include "sim/abstract_subsystem.hh"

namespace SimpleSSD::HIL::SATA {

class Subsystem : public AbstractSubsystem {
 protected:
  HIL *pHIL;

  // TODO: Only one HBA
  // TODO: Only one Device

  // Command definitions

  // Command dispatcher

 public:
  Subsystem(ObjectData &);
  virtual ~Subsystem();

  ControllerID createController(Interface *) noexcept override;
  AbstractController *getController(ControllerID) noexcept override;

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;

  Request *restoreRequest(uint64_t) noexcept override;
};

}  // namespace SimpleSSD::HIL::SATA

#endif
