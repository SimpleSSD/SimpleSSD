// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 *         Junhyeok Jang <jhjang@camelab.org>
 */

#include "ftl/base/abstract_ftl.hh"

#include "ftl/ftl.hh"

namespace SimpleSSD::FTL {

AbstractFTL::AbstractFTL(ObjectData &o, FTL *p, FIL::FIL *f,
                         Mapping::AbstractMapping *m,
                         BlockAllocator::AbstractAllocator *a)
    : Object(o), pFTL(p), pFIL(f), pMapper(m), pAllocator(a) {}

Request *AbstractFTL::getRequest(uint64_t tag) {
  return pFTL->getRequest(tag);
}

void AbstractFTL::getQueueStatus(uint64_t &nw, uint64_t &nh) noexcept {
  pFTL->getQueueStatus(nw, nh);
}

void AbstractFTL::completeRequest(Request *req) {
  pFTL->completeRequest(req);
}

uint64_t AbstractFTL::generateFTLTag() {
  return pFTL->generateFTLTag();
}

}  // namespace SimpleSSD::FTL
