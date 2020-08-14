// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "hil/none/controller.hh"

#include "hil/none/subsystem.hh"

namespace SimpleSSD::HIL::None {

Controller::Controller(ObjectData &o, ControllerID id, Subsystem *p,
                       Interface *i)
    : AbstractController(o, id, p, i) {
  panic_if(dynamic_cast<Subsystem *>(subsystem) == nullptr,
           "Invalid use of controller.");
}

Controller::~Controller() {}

HIL *Controller::getHIL() const {
  return dynamic_cast<Subsystem *>(subsystem)->getHIL();
}

uint64_t Controller::read(uint64_t, uint32_t, uint8_t *) noexcept {
  panic("Read controller register not implemented.");

  return 0;
}

uint64_t Controller::write(uint64_t, uint32_t, uint8_t *) noexcept {
  panic("Write controller register not implemented.");

  return 0;
}

void Controller::getStatList(std::vector<Stat> &, std::string) noexcept {}

void Controller::getStatValues(std::vector<double> &) noexcept {}

void Controller::resetStatValues() noexcept {}

void Controller::createCheckpoint(std::ostream &) const noexcept {}

void Controller::restoreCheckpoint(std::istream &) noexcept {}

}  // namespace SimpleSSD::HIL::None
