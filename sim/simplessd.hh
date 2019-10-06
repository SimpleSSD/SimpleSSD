// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_SIM_SIMPLESSD_HH__
#define __SIMPLESSD_SIM_SIMPLESSD_HH__

#include "sim/abstract_controller.hh"
#include "sim/config_reader.hh"
#include "sim/engine.hh"
#include "sim/interface.hh"
#include "sim/log.hh"

namespace SimpleSSD {

/**
 * \brief SimpleSSD object declaration
 *
 * SimpleSSD object. It contains everything about a SSD.
 * You can create multiple SimpleSSD objects to create multiple SSD.
 * You cannot copy this object.
 */
class SimpleSSD {
 private:
  bool inited;  //!< Flag whether this object is initialized

  ConfigReader *config;  //!< Config object provided by simulation system
  Engine *engine;        //!< Engine object provided by simulation system
  Log log;               //!< Log system

  AbstractSubsystem *subsystem;  //!< NVM Subsystem

  std::ostream *outfile;
  std::ostream *errfile;
  std::ostream *debugfile;

  void joinPath(std::string &, std::string) noexcept;
  void openStream(std::ostream *, std::string &, std::string &) noexcept;

 public:
  SimpleSSD();
  SimpleSSD(const SimpleSSD &) = delete;
  SimpleSSD(SimpleSSD &&) noexcept = default;
  ~SimpleSSD();

  SimpleSSD &operator=(const SimpleSSD &) = delete;
  SimpleSSD &operator=(SimpleSSD &&) noexcept = default;

  bool init(Engine *, ConfigReader *) noexcept;
  void deinit() noexcept;

  ControllerID createController(Interface *);
  AbstractController *getController(ControllerID = 0);

  void createCheckpoint(std::string) noexcept;
  void restoreCheckpoint(Engine *, ConfigReader *) noexcept;
};

}  // namespace SimpleSSD

#endif
