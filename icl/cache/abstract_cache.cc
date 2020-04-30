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
    : Object(o),
      sectorsInPage(MIN(p->pageSize / minIO, 1)),
      manager(m),
      parameter(p) {}

AbstractCache::~AbstractCache() {}

HIL::SubRequest *AbstractCache::getSubRequest(uint64_t tag) {
  return manager->getSubRequest(tag);
}

void AbstractCache::createCheckpoint(std::ostream &out) const noexcept {
  BACKUP_SCALAR(out, sectorsInPage);
}

void AbstractCache::restoreCheckpoint(std::istream &in) noexcept {
  uint32_t tmp32;

  RESTORE_SCALAR(in, tmp32);
  panic_if(tmp32 != sectorsInPage, "Page size mismatch.");
}

}  // namespace SimpleSSD::ICL
