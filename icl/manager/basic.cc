// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "icl/manager/basic.hh"

#include "icl/cache/abstract_cache.hh"

namespace SimpleSSD::ICL {

BasicCache::BasicCache(ObjectData &o, FTL::FTL *f) : AbstractManager(o, f) {}

BasicCache::~BasicCache() {
  if (!requestQueue.empty()) {
    warn("Request queue is not empty.");
  }
}

void BasicCache::read(SubRequest *req) {
  bool hit = false;
  LPN lpn = req->getLPN();

  requestQueue.emplace(req->getTag(), req);

  // Lookup
  auto fstat = cache->lookup(lpn, false, hit);

  if (hit) {
    // TODO: Read completion
  }
  else {
    fstat += cache->allocate(lpn, req->getTag());

    // TODO: FTL
  }

  // TODO: Handle CPU
}

void BasicCache::write(SubRequest *req) {
  bool hit = false;
  LPN lpn = req->getLPN();

  requestQueue.emplace(req->getTag(), req);

  // Lookup
  auto fstat = cache->lookup(lpn, true, hit);

  if (hit) {
    // Hit or cold-miss
    // TODO: Write completion
  }
  else {
    fstat += cache->allocate(lpn, req->getTag());

    // Continue at allocateDone
  }

  // TODO: Handle CPU
}

void BasicCache::flush(SubRequest *req) {
  auto fstat = cache->flush(req->getOffset(), req->getLength());

  requestQueue.emplace(req->getTag(), req);

  // Continue at flushDone

  // TODO: Handle CPU
}

void BasicCache::erase(SubRequest *req) {
  auto fstat = cache->erase(req->getOffset(), req->getLength());

  requestQueue.emplace(req->getTag(), req);

  // Continue at eraseDone

  // TODO: Handle CPU
}

void BasicCache::dmaDone(SubRequest *req) {
  cache->dmaDone(req->getLPN());
}

void BasicCache::drain(std::vector<FlushContext> &list) {
  // TODO: FTL
}

void BasicCache::getStatList(std::vector<Stat> &, std::string) noexcept {}

void BasicCache::getStatValues(std::vector<double> &) noexcept {}

void BasicCache::resetStatValues() noexcept {}

void BasicCache::createCheckpoint(std::ostream &) const noexcept {}

void BasicCache::restoreCheckpoint(std::istream &) noexcept {}

}  // namespace SimpleSSD::ICL
