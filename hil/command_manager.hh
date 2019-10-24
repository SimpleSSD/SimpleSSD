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

namespace SimpleSSD {

enum class Status : uint8_t {
  Prepare,   // Sub command created
  DMA,       // Sub command is in DMA
  Submit,    // Sub command issued to HIL
  Done,      // Sub command completed
  Complete,  // Sub command marked as complete

  InternalCache,      // Sub command is in ICL
  InternalCacheDone,  // Sub command is completed in ICL
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

/*
 * Tag (64bit)
 *
 * Each field is 16bit (MSB -> LSB)
 * [ Layer prefix ][ Controller ID ][ Queue ID ][ Command ID / Queue entry ID ]
 */
#define ICL_TAG_PREFIX ((uint64_t)0xFFFF000000000000ull)
#define FTL_TAG_PREFIX ((uint64_t)0xFFEE000000000000ull)

struct SubCommand {
  const uint64_t tag;
  const uint32_t id;

  Status status;

  LPN lpn;
  PPN ppn;

  uint32_t skipFront;
  uint32_t skipEnd;

  std::vector<uint8_t> buffer;
  std::vector<uint8_t> spare;

  SubCommand(uint64_t t, uint32_t i)
      : tag(t),
        id(i),
        status(Status::Prepare),
        lpn(0),
        ppn(0),
        skipFront(0),
        skipEnd(0) {}
};

struct Command {
  const uint64_t tag;

  Event eid;

  Status status;
  Operation opcode;

  LPN offset;
  LPN length;

  // Used by ICL
  uint64_t counter;
  uint64_t beginAt;

  std::vector<SubCommand> subCommandList;

  Command(uint64_t t, Event e)
      : tag(t),
        eid(e),
        status(Status::Prepare),
        opcode(Operation::None),
        offset(InvalidLPN),
        length(InvalidLPN),
        counter(0) {}
};

class CommandManager : public Object {
 private:
  std::unordered_map<uint64_t, Command> commandList;

  Command &createCommand(uint64_t, Event);
  SubCommand &createSubCommand(Command &);

 public:
  CommandManager(ObjectData &);
  CommandManager(const CommandManager &) = delete;
  CommandManager(CommandManager &&) noexcept = default;
  ~CommandManager();

  CommandManager &operator=(const CommandManager &) = delete;
  CommandManager &operator=(CommandManager &&) = default;

  Command &getCommand(uint64_t);
  std::vector<SubCommand> &getSubCommand(uint64_t);

  void destroyCommand(uint64_t);

  // Helper APIs for HIL -> ICL
  void createHILRead(uint64_t tag, Event eid, LPN slpn, LPN nlp,
                     uint32_t skipFront, uint32_t skipEnd, uint32_t lpnSize);
  void createHILWrite(uint64_t tag, Event eid, LPN slpn, LPN nlp,
                      uint32_t skipFront, uint32_t skipEnd, uint32_t lpnSize);
  void createHILFlush(uint64_t tag, Event eid, LPN slpn, LPN nlp);
  void createHILTrim(uint64_t tag, Event eid, LPN slpn, LPN nlp);
  void createHILFormat(uint64_t tag, Event eid, LPN slpn, LPN nlp);

  // Helper APIs for ICL -> FTL
  void createICLRead(uint64_t tag, Event eid, LPN slpn, LPN nlp, uint64_t now);
  void createICLWrite(uint64_t tag, Event eid, LPN slpn, LPN nlp,
                      uint32_t skipFront, uint32_t skipEnd, uint64_t now);

  // Helper APIs for FTL -> FIL
  SubCommand &appendTranslation(Command &, LPN lpn, PPN ppn);
  Command &createFTLCommand(uint64_t tag);

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD

#endif
