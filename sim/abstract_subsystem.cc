// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "sim/abstract_subsystem.hh"

#include "sim/abstract_controller.hh"

namespace SimpleSSD {

AbstractSubsystem::AbstractSubsystem(ObjectData &o) : Object(o) {}

AbstractSubsystem::~AbstractSubsystem() {}

AbstractController *AbstractSubsystem::getController(
    ControllerID cid) noexcept {
  auto iter = controllerList.find(cid);

  if (iter != controllerList.end()) {
    return iter->second.first;
  }

  return nullptr;
}

}  // namespace SimpleSSD
