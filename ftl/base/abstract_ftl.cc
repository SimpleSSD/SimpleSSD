// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "ftl/base/abstract_ftl.hh"

#include "ftl/ftl.hh"

namespace SimpleSSD::FTL {

AbstractFTL::AbstractFTL(ObjectData &o, FTLObjectData &fo, FTL *p, FIL::FIL *f)
    : Object(o), pFTL(p), ftlobject(fo), pFIL(f) {}

AbstractFTL::~AbstractFTL() {}

void AbstractFTL::completeRequest(Request *req) {
  pFTL->completeRequest(req);
}

Request *AbstractFTL::getRequest(uint64_t tag) {
  return pFTL->getRequest(tag);
}

void AbstractFTL::initialize() {}

}  // namespace SimpleSSD::FTL
