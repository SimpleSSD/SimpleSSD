// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "sim/abstract_subsystem.hh"

#include "sim/abstract_controller.hh"

namespace SimpleSSD {

ControllerData::ControllerData()
    : controller(nullptr),
      interface(nullptr),
      dma(nullptr),
      memoryPageSize(0) {}

ControllerData::ControllerData(AbstractController *c, Interface *i,
                               DMAInterface *d, uint64_t m)
    : controller(c), interface(i), dma(d), memoryPageSize(m) {}

AbstractSubsystem::AbstractSubsystem(ObjectData &o)
    : Object(o), inited(false) {}

AbstractSubsystem::~AbstractSubsystem() {}

AbstractController *AbstractSubsystem::getController(
    ControllerID cid) noexcept {
  auto iter = controllerList.find(cid);

  if (iter != controllerList.end()) {
    return iter->second->controller;
  }

  return nullptr;
}

}  // namespace SimpleSSD
