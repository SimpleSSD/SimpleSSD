// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "icl/cache/abstract_cache.hh"

#include "icl/manager/abstract_manager.hh"

namespace SimpleSSD::ICL {

AbstractCache::AbstractCache(ObjectData &o, AbstractManager *m,
                             FTL::Parameter *p)
    : Object(o), manager(m), parameter(p) {}

AbstractCache::~AbstractCache() {}

HIL::SubRequest *AbstractCache::getSubRequest(uint64_t tag) {
  return manager->getSubRequest(tag);
}

}  // namespace SimpleSSD::ICL
