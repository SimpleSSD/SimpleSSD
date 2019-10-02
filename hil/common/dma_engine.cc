// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "hil/common/dma_engine.hh"

namespace SimpleSSD::HIL {

DMAEngine::DMAEngine(ObjectData &&o, Interface *i)
    : Object(std::move(o)), pInterface(i), callCounter(0) {}

DMAEngine::~DMAEngine() {}

}  // namespace SimpleSSD::HIL
