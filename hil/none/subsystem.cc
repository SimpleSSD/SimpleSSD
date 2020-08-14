// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "hil/none/subsystem.hh"

#include "hil/none/controller.hh"

namespace SimpleSSD::HIL::None {

Subsystem::Subsystem(ObjectData &o)
    : AbstractSubsystem(o), pController(nullptr) {
  pHIL = new HIL(o, this);
}

Subsystem::~Subsystem() {
  delete pHIL;
}

HIL *Subsystem::getHIL() const {
  return pHIL;
}

void Subsystem::init() {
  // Do nothing
}

ControllerID Subsystem::createController(Interface *interface) noexcept {
  panic_if(pController != nullptr, "Only one controller is supported.");

  pController = new Controller(object, 0, this, interface);

  return 0;
}

AbstractController *Subsystem::getController(ControllerID ctrlid) noexcept {
  if (ctrlid == 0) {
    return pController;
  }

  return nullptr;
}

void Subsystem::getStatList(std::vector<Stat> &list,
                            std::string prefix) noexcept {
  auto nprefix = prefix + "hil.none.";

  if (pController) {
    pController->getStatList(list, nprefix + "ctrl.");
  }

  pHIL->getStatList(list, prefix);
}

void Subsystem::getStatValues(std::vector<double> &values) noexcept {
  if (pController) {
    pController->getStatValues(values);
  }

  pHIL->getStatValues(values);
}

void Subsystem::resetStatValues() noexcept {
  if (pController) {
    pController->resetStatValues();
  }

  pHIL->resetStatValues();
}

void Subsystem::createCheckpoint(std::ostream &out) const noexcept {
  pHIL->createCheckpoint(out);
}

void Subsystem::restoreCheckpoint(std::istream &in) noexcept {
  pHIL->restoreCheckpoint(in);
}

Request *Subsystem::restoreRequest(uint64_t) noexcept {
  panic("Checkpoint not works.");

  return nullptr;
}

}  // namespace SimpleSSD::HIL::None
