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
      parameter(p) {
  auto evictMode = (Config::Granularity)readConfigUint(
      Section::InternalCache, Config::Key::EvictGranularity);

  // # pages to evict once
  switch (evictMode) {
    case Config::Granularity::FirstLevel:
      pagesToEvict = parameter->parallelismLevel[0];

      break;
    case Config::Granularity::SecondLevel:
      pagesToEvict = parameter->parallelismLevel[0];
      pagesToEvict *= parameter->parallelismLevel[1];

      break;
    case Config::Granularity::ThirdLevel:
      pagesToEvict = parameter->parallelismLevel[0];
      pagesToEvict *= parameter->parallelismLevel[1];
      pagesToEvict *= parameter->parallelismLevel[2];

      break;
    case Config::Granularity::AllLevel:
      pagesToEvict = parameter->parallelism;

      break;
    default:
      panic("Unexpected eviction granularity.");

      break;
  }
}

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
