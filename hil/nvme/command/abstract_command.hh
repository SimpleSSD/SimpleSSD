// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_HIL_NVME_COMMAND_ABSTRACT_COMMAND_HH__
#define __SIMPLESSD_HIL_NVME_COMMAND_ABSTRACT_COMMAND_HH__

#include <unordered_map>

#include "hil/common/dma_engine.hh"
#include "hil/common/interrupt_manager.hh"
#include "sim/interface.hh"
#include "sim/object.hh"
#include "util/algorithm.hh"

namespace SimpleSSD::HIL::NVMe {

#define debugprint_command(tag, format, ...)                                   \
  {                                                                            \
    uint64_t _uid = tag->getGCID();                                            \
                                                                               \
    debugprint(Log::DebugID::HIL_NVMe_Command,                                 \
               "CTRL %-3u | SQ %2u:%-5u | " format, HIGH32(_uid),              \
               HIGH16(_uid), LOW16(_uid), ##__VA_ARGS__);                      \
  }

class Subsystem;
class Controller;
class Arbitrator;
class SQContext;
class CQContext;
class ControllerData;
class CommandData;
class DMACommandData;
class BufferCommandData;

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

  /**
   * \brief Pending command list
   *
   * As command can be completed out-of-order, we cannot handle completion
   * events with simple queue (queue, list or deque).
   * As checkpoint recovery may change pointer to CommandData, we need to store
   * both command unique ID and Tag itself.
   */
  std::unordered_map<uint64_t, CommandTag> tagList;

  CommandTag createTag(ControllerData *, SQContext *);
  DMACommandData *createDMATag(ControllerData *, SQContext *);
  BufferCommandData *createBufferTag(ControllerData *, SQContext *);

  CommandTag findTag(uint64_t);
  DMACommandData *findDMATag(uint64_t);
  BufferCommandData *findBufferTag(uint64_t);

  void destroyTag(CommandTag);
  void addTagToList(CommandTag);

 public:
  Command(ObjectData &, Subsystem *);
  Command(const Command &) = delete;
  Command(Command &&) noexcept = delete;

  virtual void setRequest(ControllerData *, SQContext *) = 0;
  void completeRequest(CommandTag);

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::HIL::NVMe

#endif
