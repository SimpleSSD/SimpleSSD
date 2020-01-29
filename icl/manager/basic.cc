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
      createEvent([this](uint64_t t, uint64_t d) { eraseDone(t, d); },
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

void BasicCache::eraseDone(uint64_t now, uint64_t tag) {
  auto iter = requestQueue.find(tag);

  panic_if(iter == requestQueue.end(), "Unexpected SubRequest ID %" PRIu64 ".",
           tag);

  auto &req = *iter->second;

  scheduleAbs(eventICLCompletion, tag, now);

  requestQueue.erase(iter);
}

void BasicCache::read(SubRequest *req) {
  bool hit = false;
  LPN lpn = req->getLPN();

  requestQueue.emplace(req->getTag(), req);

  // Lookup
  auto fstat = cache->lookup(lpn, false, hit);

  if (hit) {
    // Cache hit, immediate completion
    req->setHit();
  }
  else {
    fstat += cache->allocate(lpn, req->getTag());

    // Continue at allocateDone
  }

  scheduleFunction(CPU::CPUGroup::InternalCache, eventLookupDone, req->getTag(),
                   fstat);
}

void BasicCache::write(SubRequest *req) {
  bool hit = false;
  LPN lpn = req->getLPN();
  auto tag = req->getTag();

  requestQueue.emplace(tag, req);

  // Lookup
  auto fstat = cache->lookup(lpn, true, hit);

  if (hit) {
    // Hit or cold-miss
    req->setHit();
  }
  else {
    fstat += cache->allocate(lpn, tag);

    // Continue at allocateDone
  }

  scheduleFunction(CPU::CPUGroup::InternalCache, eventLookupDone, tag, fstat);
}

void BasicCache::flush(SubRequest *req) {
  auto tag = req->getTag();
  auto fstat = cache->flush(req->getOffset(), req->getLength());

  requestQueue.emplace(tag, req);

  // Continue at flushDone

  scheduleFunction(CPU::CPUGroup::InternalCache, InvalidEventID, tag, fstat);
}

void BasicCache::erase(SubRequest *req) {
  auto tag = req->getTag();
  auto fstat = cache->erase(req->getOffset(), req->getLength());

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

  auto &req = *iter->second;

  requestQueue.erase(iter);
}

void BasicCache::flushDone(uint64_t tag) {
  auto iter = requestQueue.find(tag);

  panic_if(iter == requestQueue.end(), "Unexpected SubRequest ID %" PRIu64 ".",
           tag);

  auto &req = *iter->second;

  requestQueue.erase(iter);
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
