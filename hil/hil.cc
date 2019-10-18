// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "hil/hil.hh"

namespace SimpleSSD::HIL {

HIL::HIL(ObjectData &o)
    : Object(o),
      commandManager(object),
      icl(object, &commandManager),
      requestCounter(0) {
  logicalPageSize = icl.getLPNSize();
}

HIL::~HIL() {}

void HIL::submitCommand(uint64_t tag) {
  auto &cmd = commandManager.getCommand(tag);
  uint32_t size = cmd.subCommandList.size();

  cmd.status = Status::Submit;

  for (uint32_t i = 0; i < size; i++) {
    icl.submit(tag, i);
  }
}

void HIL::submitSubcommand(uint64_t tag, uint32_t id) {
  commandManager.getCommand(tag).status = Status::Submit;

  icl.submit(tag, id);
}

LPN HIL::getPageUsage(LPN offset, LPN length) {
  return icl.getPageUsage(offset, length);
}

LPN HIL::getTotalPages() {
  return icl.getTotalPages();
}

uint64_t HIL::getLPNSize() {
  return logicalPageSize;
}

void HIL::setCache(bool set) {
  icl.setCache(set);
}

bool HIL::getCache() {
  return icl.getCache();
}

CommandManager *HIL::getCommandManager() {
  return &commandManager;
}

void HIL::getStatList(std::vector<Stat> &list, std::string prefix) noexcept {
  icl.getStatList(list, prefix);
}

void HIL::getStatValues(std::vector<double> &values) noexcept {
  icl.getStatValues(values);
}

void HIL::resetStatValues() noexcept {
  icl.resetStatValues();
}

void HIL::createCheckpoint(std::ostream &out) const noexcept {
  BACKUP_SCALAR(out, requestCounter);

  commandManager.createCheckpoint(out);
  icl.createCheckpoint(out);
}

void HIL::restoreCheckpoint(std::istream &in) noexcept {
  RESTORE_SCALAR(in, requestCounter);

  commandManager.restoreCheckpoint(in);
  icl.restoreCheckpoint(in);

  // Just update this value here
  logicalPageSize = icl.getLPNSize();
}

}  // namespace SimpleSSD::HIL
