// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "ftl/read_reclaim/abstract_read_reclaim.hh"

#include "ftl/allocator/abstract_allocator.hh"
#include "ftl/mapping/abstract_mapping.hh"

namespace SimpleSSD::FTL::ReadReclaim {

AbstractReadReclaim::AbstractReadReclaim(ObjectData &o, FTLObjectData &fo,
                                         FIL::FIL *fil)
    : AbstractJob(o, fo),
      pFIL(fil),
      state(State::Idle),
      pageSize(object.config->getNANDStructure()->pageSize),
      param(nullptr),
      engine(std::random_device{}()) {}

AbstractReadReclaim::~AbstractReadReclaim() {}

uint32_t AbstractReadReclaim::estimateBitError(uint64_t now, const PPN &ppn) {
  auto pspn = param->getPSPNFromPPN(ppn);
  auto psbn = param->getPSBNFromPSPN(pspn);

  auto &bmeta = ftlobject.pAllocator->getBlockMetadata(psbn);

  // TODO: Move this model to FIL
  double rber = 0.;

  {
    // 2y-nm MLC, because we are using intel750
    const double e = 8.34e-05;
    const double alpha = 3.30e-11;
    const double beta = 5.56e-19;
    const double gamma = 6.26e-13;
    const double k = 1.71;
    const double m = 2.49;
    const double n = 3.33;
    const double p = 1.76;
    const double q = 0.47;

    const double cycles = (double)bmeta.erasedCount;
    const double time =
        (double)(now - bmeta.insertedAt) / 1000000000000. / 86400.;
    const double reads = (double)bmeta.readCountAfterErase;

    rber = e + alpha * pow(cycles, k) +             // wear
           beta * pow(cycles, m) * pow(time, n) +   // retention
           gamma * pow(cycles, p) * pow(reads, q);  // disturbance
  }

  return std::binomial_distribution<uint32_t>{pageSize, rber}(engine);
}

void AbstractReadReclaim::initialize() {
  param = ftlobject.pMapping->getInfo();
}

bool AbstractReadReclaim::isRunning() {
  return state > State::Idle;
}

void AbstractReadReclaim::triggerByUser(TriggerType when, Request *req) {
  if (when == TriggerType::ReadComplete) {
    doErrorCheck(req->getPPN());
  }
}

}  // namespace SimpleSSD::FTL::ReadReclaim
