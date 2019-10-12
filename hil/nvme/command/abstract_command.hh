// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_HIL_NVME_COMMAND_ABSTRACT_COMMAND_HH__
#define __SIMPLESSD_HIL_NVME_COMMAND_ABSTRACT_COMMAND_HH__

#include <deque>

#include "hil/common/dma_engine.hh"
#include "hil/common/interrupt_manager.hh"
#include "sim/interface.hh"
#include "sim/object.hh"

namespace SimpleSSD::HIL::NVMe {

#define debugprint_command(format, ...)                                        \
  {                                                                            \
    uint64_t _uid = getUniqueID();                                             \
                                                                               \
    debugprint(Log::DebugID::HIL_NVMe_Command,                                 \
               "CTRL %-3u | SQ %2u:%-5u | " format, (uint16_t)(_uid >> 32),    \
               (uint16_t)(_uid >> 16), (uint16_t)_uid, ##__VA_ARGS__);         \
  }

class Subsystem;
class Controller;
class Arbitrator;
class SQContext;
class CQContext;
class ControllerData;
class CommandData;

using CommandTag = CommandData *;

const CommandTag InvalidCommandTag = nullptr;

/**
 * \brief Command object
 *
 * All NVMe command inherits this object.
 */
class Command : public Object {
 protected:
  Subsystem *subsystem;

 public:
  Command(ObjectData &, Subsystem *);
  Command(const Command &) = delete;
  Command(Command &&) noexcept = delete;
  virtual ~Command();

  virtual CommandTag setRequest(ControllerData *, SQContext *) = 0;
  virtual void completeRequest(CommandTag);

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;
};

}  // namespace SimpleSSD::HIL::NVMe

#endif
