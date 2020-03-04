// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "icl/manager/basic.hh"

#include "icl/cache/abstract_cache.hh"

namespace SimpleSSD::ICL {

BasicCache::BasicCache(ObjectData &o, ICL::ICL *p, FTL::FTL *f)
    : AbstractManager(o, p, f) {
  eventDrainDone = createEvent([this](uint64_t, uint64_t d) { drainDone(d); },
                               "ICL::BasicCache::eventDrainDone");
}

BasicCache::~BasicCache() {}

void BasicCache::read(HIL::SubRequest *req) {
  cache->lookup(req);
}

void BasicCache::write(HIL::SubRequest *req) {
  cache->lookup(req);
}

void BasicCache::flush(HIL::SubRequest *req) {
  cache->flush(req);
}

void BasicCache::erase(HIL::SubRequest *req) {
  cache->erase(req);
}

void BasicCache::dmaDone(HIL::SubRequest *req) {
  cache->dmaDone(req->getLPN());
}

void BasicCache::lookupDone(uint64_t tag) {
  auto req = getSubRequest(tag);

  if (req->getAllocate()) {
    // We need allocation
    cache->allocate(req);
  }
  else {
    cacheDone(tag);
  }
}

void BasicCache::cacheDone(uint64_t tag) {
  scheduleNow(eventICLCompletion, tag);
}

void BasicCache::drain(std::vector<FlushContext> &list) {
  // TODO: FTL end with drainDone
}

void BasicCache::drainDone(uint64_t tag) {}

void BasicCache::getStatList(std::vector<Stat> &, std::string) noexcept {}

void BasicCache::getStatValues(std::vector<double> &) noexcept {}

void BasicCache::resetStatValues() noexcept {}

void BasicCache::createCheckpoint(std::ostream &out) const noexcept {}

void BasicCache::restoreCheckpoint(std::istream &in) noexcept {}

}  // namespace SimpleSSD::ICL
