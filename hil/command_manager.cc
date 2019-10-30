// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "hil/command_manager.hh"

namespace SimpleSSD {

CommandManager::CommandManager(ObjectData &o) : Object(o) {}

CommandManager::~CommandManager() {
  /*
   * Cache may have uncompleted command tags -- as they read/write request in
   * background, simulation may terminated before all write requests are
   * completed by FTL and FIL.
   */
  warn_if(commandList.size() > 0, "Not all commands are destroyed.");
}

Command &CommandManager::getCommand(uint64_t tag) {
  auto iter = commandList.find(tag);

  panic_if(iter == commandList.end(), "No such command exists.");

  return iter->second;
}

std::vector<SubCommand> &CommandManager::getSubCommand(uint64_t tag) {
  auto iter = commandList.find(tag);

  panic_if(iter == commandList.end(), "No such command exists.");

  return iter->second.subCommandList;
}

Command &CommandManager::createCommand(uint64_t tag, Event eid) {
  auto iter = commandList.emplace(tag, Command(tag, eid));

  panic_if(!iter.second, "Command with tag %" PRIu64 " already exists.", tag);

  return iter.first->second;
}

SubCommand &CommandManager::createSubCommand(Command &cmd) {
  uint32_t id = (uint32_t)cmd.subCommandList.size();

  return cmd.subCommandList.emplace_back(SubCommand(cmd.tag, id));
}

void CommandManager::destroyCommand(uint64_t tag) {
  auto iter = commandList.find(tag);

  panic_if(iter == commandList.end(), "No such command exists.");

  commandList.erase(iter);
}

void CommandManager::createHILRead(uint64_t tag, Event eid, LPN slpn, LPN nlp,
                                   uint32_t skipFront, uint32_t skipEnd,
                                   uint32_t lpnSize) {
  auto &cmd = createCommand(tag, eid);

  cmd.opcode = Operation::Read;
  cmd.offset = slpn;
  cmd.length = nlp;

  cmd.subCommandList.reserve(nlp);

  for (LPN i = slpn; i < slpn + nlp; i++) {
    auto &scmd = createSubCommand(cmd);

    scmd.lpn = i;

    if (i == slpn) {
      scmd.skipFront = skipFront;
    }
    if (i + 1 == slpn + nlp) {
      scmd.skipEnd = skipEnd;
    }

    scmd.buffer.resize(lpnSize);
  }
}

void CommandManager::createHILWrite(uint64_t tag, Event eid, LPN slpn, LPN nlp,
                                    uint32_t skipFront, uint32_t skipEnd,
                                    uint32_t lpnSize) {
  auto &cmd = createCommand(tag, eid);

  cmd.opcode = Operation::Write;
  cmd.offset = slpn;
  cmd.length = nlp;

  cmd.subCommandList.reserve(nlp);

  for (LPN i = slpn; i < slpn + nlp; i++) {
    auto &scmd = createSubCommand(cmd);

    scmd.lpn = i;

    if (i == slpn) {
      scmd.skipFront = skipFront;
    }
    if (i + 1 == slpn + nlp) {
      scmd.skipEnd = skipEnd;
    }

    scmd.buffer.resize(lpnSize);
  }
}

void CommandManager::createHILFlush(uint64_t tag, Event eid, LPN slpn,
                                    LPN nlp) {
  auto &cmd = createCommand(tag, eid);

  cmd.opcode = Operation::Flush;
  cmd.offset = slpn;
  cmd.length = nlp;
}

void CommandManager::createHILTrim(uint64_t tag, Event eid, LPN slpn, LPN nlp) {
  auto &cmd = createCommand(tag, eid);

  cmd.opcode = Operation::Trim;
  cmd.offset = slpn;
  cmd.length = nlp;
}

void CommandManager::createHILFormat(uint64_t tag, Event eid, LPN slpn,
                                     LPN nlp) {
  auto &cmd = createCommand(tag, eid);

  cmd.opcode = Operation::Format;
  cmd.offset = slpn;
  cmd.length = nlp;
}

void CommandManager::createICLRead(uint64_t tag, Event eid, LPN slpn, LPN nlp,
                                   uint64_t now) {
  auto &cmd = createCommand(tag, eid);

  cmd.opcode = Operation::Read;
  cmd.offset = slpn;
  cmd.length = nlp;
  cmd.beginAt = now;

  cmd.subCommandList.reserve(nlp);

  for (LPN i = slpn; i < slpn + nlp; i++) {
    auto &scmd = createSubCommand(cmd);

    scmd.lpn = i;
  }
}

void CommandManager::createICLWrite(uint64_t tag, Event eid, LPN slpn, LPN nlp,
                                    uint32_t skipFront, uint32_t skipEnd,
                                    uint64_t now) {
  auto &cmd = createCommand(tag, eid);

  cmd.opcode = Operation::Write;
  cmd.offset = slpn;
  cmd.length = nlp;
  cmd.beginAt = now;

  cmd.subCommandList.reserve(nlp);

  for (LPN i = slpn; i < slpn + nlp; i++) {
    auto &scmd = createSubCommand(cmd);

    scmd.lpn = i;

    if (i == slpn) {
      scmd.skipFront = skipFront;
    }
    if (i + 1 == slpn + nlp) {
      scmd.skipEnd = skipEnd;
    }
  }
}

SubCommand &CommandManager::appendTranslation(Command &cmd, LPN lpn, PPN ppn) {
  auto &scmd = createSubCommand(cmd);

  panic_if(lpn != InvalidLPN && scmd.id != lpn - cmd.offset,
           "Invalid LPN specified.");

  scmd.lpn = lpn;
  scmd.ppn = ppn;

  return scmd;
}

Command &CommandManager::createFTLCommand(uint64_t tag) {
  return createCommand(tag, InvalidEventID);
}

void CommandManager::getStatList(std::vector<Stat> &, std::string) noexcept {}

void CommandManager::getStatValues(std::vector<double> &) noexcept {}

void CommandManager::resetStatValues() noexcept {}

void CommandManager::createCheckpoint(std::ostream &out) const noexcept {
  uint64_t size = commandList.size();

  BACKUP_SCALAR(out, size);

  for (auto &iter : commandList) {
    BACKUP_SCALAR(out, iter.first);

    BACKUP_EVENT(out, iter.second.eid);
    BACKUP_SCALAR(out, iter.second.status);
    BACKUP_SCALAR(out, iter.second.opcode);
    BACKUP_SCALAR(out, iter.second.offset);
    BACKUP_SCALAR(out, iter.second.length);
    BACKUP_SCALAR(out, iter.second.counter);

    size = iter.second.subCommandList.size();
    BACKUP_SCALAR(out, size);

    for (auto &scmd : iter.second.subCommandList) {
      BACKUP_SCALAR(out, scmd.status);
      BACKUP_SCALAR(out, scmd.lpn);
      BACKUP_SCALAR(out, scmd.ppn);
      BACKUP_SCALAR(out, scmd.skipFront);
      BACKUP_SCALAR(out, scmd.skipEnd);

      size = scmd.buffer.size();
      BACKUP_SCALAR(out, size);

      if (size > 0) {
        BACKUP_BLOB(out, scmd.buffer.data(), size);
      }

      size = scmd.spare.size();
      BACKUP_SCALAR(out, size);

      if (size > 0) {
        BACKUP_BLOB(out, scmd.spare.data(), size);
      }
    }
  }
}

void CommandManager::restoreCheckpoint(std::istream &in) noexcept {
  uint64_t size, size2, size3;

  RESTORE_SCALAR(in, size);

  for (uint64_t i = 0; i < size; i++) {
    uint64_t tag;
    Event eid;

    RESTORE_SCALAR(in, tag);
    RESTORE_EVENT(in, eid);

    Command cmd(tag, eid);

    RESTORE_SCALAR(in, cmd.status);
    RESTORE_SCALAR(in, cmd.opcode);
    RESTORE_SCALAR(in, cmd.offset);
    RESTORE_SCALAR(in, cmd.length);
    RESTORE_SCALAR(in, cmd.counter);

    RESTORE_SCALAR(in, size2);

    cmd.subCommandList.reserve(size2);

    for (uint64_t j = 0; j < size2; j++) {
      SubCommand scmd(tag, (uint32_t)j);

      RESTORE_SCALAR(in, scmd.status);
      RESTORE_SCALAR(in, scmd.lpn);
      RESTORE_SCALAR(in, scmd.ppn);
      RESTORE_SCALAR(in, scmd.skipFront);
      RESTORE_SCALAR(in, scmd.skipEnd);

      RESTORE_SCALAR(in, size3);

      if (size3 > 0) {
        scmd.buffer.resize(size3);

        RESTORE_BLOB(in, scmd.buffer.data(), size);
      }

      RESTORE_SCALAR(in, size3);

      if (size3 > 0) {
        scmd.spare.resize(size3);

        RESTORE_BLOB(in, scmd.spare.data(), size);
      }

      cmd.subCommandList.emplace_back(scmd);
    }

    commandList.emplace(tag, cmd);
  }
}

}  // namespace SimpleSSD
