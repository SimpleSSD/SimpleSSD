// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "hil/common/dma_engine.hh"

namespace SimpleSSD::HIL {

DMAEngine::DMAContext::DMAContext(Event e)
    : counter(0), eid(e), context(nullptr) {}

DMAEngine::DMAContext::DMAContext(Event e, void *c)
    : counter(0), eid(e), context(c) {}

DMAEngine::DMAEngine(ObjectData &&o, Interface *i) : Object(o), pInterface(i) {
  dmaHandler =
      createEvent([this](uint64_t t, void *c) { dmaDone(t, (DMAContext *)c); },
                  "HIL::DMAEngine::dmaHandler");
}

DMAEngine::~DMAEngine() {}

void DMAEngine::dmaDone(uint64_t tick, DMAContext *context) {
  context->counter--;

  if (context->counter == 0) {
    schedule(context->eid, tick, context->context);
    delete context;
  }
}

}  // namespace SimpleSSD::HIL
