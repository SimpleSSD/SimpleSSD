// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
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

}  // namespace SimpleSSD::FTL
