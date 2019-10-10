// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "hil/common/dma_engine.hh"

namespace SimpleSSD::HIL {

DMAEngine::DMAContext::DMAContext() : counter(0), eid(InvalidEventID) {}

DMAEngine::DMAContext::DMAContext(Event e) : counter(0), eid(e) {}

DMAEngine::DMAEngine(ObjectData &o, DMAInterface *i)
    : Object(o), pInterface(i) {
  dmaHandler = createEvent([this](uint64_t) { dmaDone(); },
                           "HIL::DMAEngine::dmaHandler");
}

DMAEngine::~DMAEngine() {}

void DMAEngine::dmaDone() {
  dmaContext.counter--;

  if (dmaContext.counter == 0) {
    schedule(dmaContext.eid);
  }
}

void DMAEngine::getStatList(std::vector<Stat> &, std::string) noexcept {}

void DMAEngine::getStatValues(std::vector<double> &) noexcept {}

void DMAEngine::resetStatValues() noexcept {}

void DMAEngine::createCheckpoint(std::ostream &out) const noexcept {
  BACKUP_SCALAR(out, dmaContext);
}

void DMAEngine::restoreCheckpoint(std::istream &in) noexcept {
  RESTORE_SCALAR(in, dmaContext);
}

}  // namespace SimpleSSD::HIL
