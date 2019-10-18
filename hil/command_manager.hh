// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_HIL_COMMAND_MANAGER_HH__
#define __SIMPLESSD_HIL_COMMAND_MANAGER_HH__

#include <unordered_map>
#include <vector>

#include "sim/object.hh"

namespace SimpleSSD::HIL {

enum class Status : uint8_t {
  Submit,
  Done,
  Complete,

};

enum class Operation : uint8_t {
  None,
  Read,
  Write,
  Erase,
  Flush,
  Trim,
  Format,
};

struct SubCommand {
  Status status;
  Operation opcode;

  LPN lpn;
  PPN ppn;

  uint32_t skipFront;
  uint32_t skipEnd;

  std::vector<uint8_t> buffer;
  std::vector<uint8_t> spare;

  SubCommand()
      : status(Status::Complete),
        opcode(Operation::None),
        lpn(0),
        ppn(0),
        skipFront(0),
        skipEnd(0) {}
};

struct Command {
  Event eid;

  LPN offset;
  LPN length;

  std::vector<SubCommand> subCommandList;

  Command(Event e) : eid(e) {}
};

class CommandManager : public Object {
 private:
  std::unordered_map<uint64_t, Command> commandList;

 public:
  CommandManager(ObjectData &);
  CommandManager(const CommandManager &) = delete;
  CommandManager(CommandManager &&) noexcept = default;
  ~CommandManager();

  CommandManager &operator=(const CommandManager &) = delete;
  CommandManager &operator=(CommandManager &&) = default;

  std::vector<SubCommand> &getSubCommand(uint64_t);

  void createCommand(uint64_t, Event);
  uint32_t createSubCommand(uint64_t);
  void destroyCommand(uint64_t);

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::HIL

#endif
