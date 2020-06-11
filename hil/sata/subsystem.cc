// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "hil/sata/subsystem.hh"

namespace SimpleSSD::HIL::SATA {

Subsystem::Subsystem(ObjectData &o) : AbstractSubsystem(o) {
  pHIL = new HIL(o, this);
}

Subsystem::~Subsystem() {
  delete pHIL;
}

}  // namespace SimpleSSD::HIL::SATA
