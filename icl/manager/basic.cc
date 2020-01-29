// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "icl/manager/basic.hh"

#include "icl/cache/abstract_cache.hh"

namespace SimpleSSD::ICL {

BasicCache::BasicCache(ObjectData &o, FTL::FTL *f) : AbstractManager(o, f) {
  // Create events
  eventLookupDone =
      createEvent([this](uint64_t t, uint64_t d) { lookupDone(t, d); },
                  "ICL::BasicCache::eventLookupDone");
  eventEraseDone =
      createEvent([this](uint64_t, uint64_t d) { allocateDone(d); },
                  "ICL::BasicCache::eventEraseDone");
}

BasicCache::~BasicCache() {
  if (!requestQueue.empty()) {
    warn("Request queue is not empty.");
  }
}

void BasicCache::lookupDone(uint64_t now, uint64_t tag) {
  auto iter = requestQueue.find(tag);

  panic_if(iter == requestQueue.end(), "Unexpected SubRequest ID %" PRIu64 ".",
           tag);

  auto &req = *iter->second;

  if (req.getHit()) {
    scheduleAbs(eventICLCompletion, tag, now);

    requestQueue.erase(iter);
  }

  // If not hit, just wait for allocateDone()
}

void BasicCache::read(SubRequest *req) {
  LPN lpn = req->getLPN();
  auto tag = req->getTag();

  requestQueue.emplace(tag, req);

  // Lookup
  auto fstat = cache->lookup(req, false);

  if (!req->getHit()) {
    // Cache miss
    fstat += cache->allocate(req);

    // Continue at allocateDone
  }

  scheduleFunction(CPU::CPUGroup::InternalCache, eventLookupDone, tag, fstat);
}

void BasicCache::write(SubRequest *req) {
  LPN lpn = req->getLPN();
  auto tag = req->getTag();

  requestQueue.emplace(tag, req);

  // Lookup
  auto fstat = cache->lookup(req, true);

  if (!req->getHit()) {
    // Capcity-miss or conflict-miss
    fstat += cache->allocate(req);

    // Continue at allocateDone
  }

  scheduleFunction(CPU::CPUGroup::InternalCache, eventLookupDone, tag, fstat);
}

void BasicCache::flush(SubRequest *req) {
  auto tag = req->getTag();
  auto fstat = cache->flush(req);

  requestQueue.emplace(tag, req);

  // Continue at flushDone

  scheduleFunction(CPU::CPUGroup::InternalCache, InvalidEventID, tag, fstat);
}

void BasicCache::erase(SubRequest *req) {
  auto tag = req->getTag();
  auto fstat = cache->erase(req);

  requestQueue.emplace(tag, req);

  scheduleFunction(CPU::CPUGroup::InternalCache, eventEraseDone, tag, fstat);
}

void BasicCache::dmaDone(SubRequest *req) {
  cache->dmaDone(req->getLPN());
}

void BasicCache::allocateDone(uint64_t tag) {
  auto iter = requestQueue.find(tag);

  panic_if(iter == requestQueue.end(), "Unexpected SubRequest ID %" PRIu64 ".",
           tag);

  scheduleNow(eventICLCompletion, tag);

  requestQueue.erase(iter);
}

void BasicCache::flushDone(uint64_t tag) {
  allocateDone(tag);
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
