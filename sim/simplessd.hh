// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_SIM_SIMPLESSD_HH__
#define __SIMPLESSD_SIM_SIMPLESSD_HH__

#include <filesystem>

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

  ObjectData object;
  Log log;  //!< Log system

  AbstractSubsystem *subsystem;  //!< NVM Subsystem

  std::ostream *outfile;
  std::ostream *errfile;
  std::ostream *debugfile;

  std::ostream *openStream(std::string &, std::string &) noexcept;
  void closeStream(std::ostream *) noexcept;

  bool comparePath(std::string &, std::string &, std::string &) noexcept;

  template <class... T>
  inline void debugprint(Log::DebugID id, const char *format,
                         T... args) const noexcept {
    object.log->debugprint(id, format, args...);
  }

 public:
  SimpleSSD();
  SimpleSSD(const SimpleSSD &) = delete;
  SimpleSSD(SimpleSSD &&) noexcept = default;
  ~SimpleSSD();

  SimpleSSD &operator=(const SimpleSSD &) = delete;
  SimpleSSD &operator=(SimpleSSD &&) = default;

  bool init(Engine *, ConfigReader *) noexcept;
  void deinit() noexcept;

  ControllerID createController(Interface *);
  AbstractController *getController(ControllerID = 0);
  ObjectData &getObject();

  void getStatList(std::vector<Stat> &, std::string) noexcept;
  void getStatValues(std::vector<double> &) noexcept;
  void resetStatValues() noexcept;

  void createCheckpoint(const std::filesystem::path &) const noexcept;
  void restoreCheckpoint(const std::filesystem::path &) noexcept;
};

}  // namespace SimpleSSD

#endif
