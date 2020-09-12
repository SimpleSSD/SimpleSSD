// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_HIL_NONE_SUBSYSTEM_HH__
#define __SIMPLESSD_HIL_NONE_SUBSYSTEM_HH__

#include "hil/hil.hh"
#include "sim/abstract_subsystem.hh"

namespace SimpleSSD::HIL::None {

class Controller;

class Subsystem : public AbstractSubsystem {
 protected:
  HIL *pHIL;
  Controller *pController;

 public:
  Subsystem(ObjectData &);
  virtual ~Subsystem();

  HIL *getHIL() const;

  void init() override;

  ControllerID createController(Interface *) noexcept override;
  AbstractController *getController(ControllerID) noexcept override;

  void getGCHint(FTL::GC::HintContext &ctx) noexcept override;

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;

  Request *restoreRequest(uint64_t) noexcept override;
};

}  // namespace SimpleSSD::HIL::None

#endif
