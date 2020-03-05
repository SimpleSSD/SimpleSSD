// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "icl/manager/basic.hh"

#include "icl/cache/abstract_cache.hh"

namespace SimpleSSD::ICL {

BasicDetector::BasicDetector(uint32_t p, uint64_t c, uint64_t r)
    : SequentialDetector(p),
      lastRequestTag(1),
      lpn(std::numeric_limits<LPN>::max()),
      offset(0),
      length(0),
      hitCounter(0),
      accessCounter(0),
      triggerCount(c),
      triggerRatio(r) {}

void BasicDetector::submitSubRequest(HIL::SubRequest *req) {
  if (lastRequestTag != req->getParentTag()) {
    if (lpn * pageSize + offset + length == req->getLPN() * pageSize + offset) {
      if (!enabled) {
        hitCounter++;
        accessCounter += length;

        if (hitCounter >= triggerCount &&
            accessCounter / pageSize >= triggerRatio) {
          enabled = true;
        }
      }
    }
    else {
      enabled = false;
      hitCounter = 0;
      accessCounter = 0;
    }
  }

  lpn = req->getLPN();

  LPN diff = (lpn - req->getSLPN()) * pageSize;

  offset = req->getOffset() - diff;
  length = req->getLength();
}

void BasicDetector::createCheckpoint(std::ostream &out) const noexcept {
  SequentialDetector::createCheckpoint(out);

  BACKUP_SCALAR(out, lastRequestTag);
  BACKUP_SCALAR(out, lpn);
  BACKUP_SCALAR(out, offset);
  BACKUP_SCALAR(out, length);
  BACKUP_SCALAR(out, hitCounter);
  BACKUP_SCALAR(out, accessCounter);
}

void BasicDetector::restoreCheckpoint(std::istream &in,
                                      ObjectData &object) noexcept {
  SequentialDetector::restoreCheckpoint(in, object);

  RESTORE_SCALAR(in, lastRequestTag);
  RESTORE_SCALAR(in, lpn);
  RESTORE_SCALAR(in, offset);
  RESTORE_SCALAR(in, length);
  RESTORE_SCALAR(in, hitCounter);
  RESTORE_SCALAR(in, accessCounter);
}

BasicCache::BasicCache(ObjectData &o, ICL::ICL *p, FTL::FTL *f)
    : AbstractManager(o, p, f), detector(nullptr), drainCounter(0) {
  auto ftlparam = f->getInfo();
  bool enable =
      readConfigBoolean(Section::InternalCache, Config::Key::EnablePrefetch);

  if (enable) {
    // Create sequential I/O detector
    auto count =
        readConfigUint(Section::InternalCache, Config::Key::PrefetchCount);
    auto ratio =
        readConfigUint(Section::InternalCache, Config::Key::PrefetchRatio);

    detector = new BasicDetector(ftlparam->pageSize, count, ratio);
  }

  auto prefetchMode = (Config::Granularity)readConfigUint(
      Section::InternalCache, Config::Key::PrefetchMode);

  prefetchTrigger = std::numeric_limits<LPN>::max();
  lastPrefetched = 0;

  prefetchCount = prefetchMode == Config::Granularity::AllLevel
                      ? ftlparam->parallelism
                      : ftlparam->parallelismLevel[0];

  prefetched = 0;
  drained = 0;

  eventDrainDone = createEvent([this](uint64_t, uint64_t d) { drainDone(d); },
                               "ICL::BasicCache::eventDrainDone");
}

BasicCache::~BasicCache() {
  if (detector) {
    delete detector;
  }
}

void BasicCache::read(HIL::SubRequest *req) {
  cache->lookup(req);

  if (detector) {
    bool old = detector->isEnabled();

    detector->submitSubRequest(req);

    if (detector->isEnabled()) {
      LPN nextlpn = req->getLPN() + 1;

      if (old) {
        // Continued - prefetch
        if (nextlpn < prefetchTrigger) {
          // Not yet
          return;
        }

        nextlpn = lastPrefetched;
      }
      else {
        // First - read-ahead
      }

      // Make prefetch request
      LPN begin = nextlpn;
      LPN end = begin + prefetchCount;

      prefetchTrigger = (begin + end) / 2;
      lastPrefetched = end;

      prefetched += prefetchCount;

      pICL->makeRequest(begin, end);
    }
  }
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
  auto req = getSubRequest(tag);
  auto opcode = req->getOpcode();

  if (opcode == HIL::Operation::Trim || opcode == HIL::Operation::Format) {
    // Submit to FIL
    pFTL->invalidate(FTL::Request(eventICLCompletion, req));
  }
  else {
    scheduleNow(eventICLCompletion, tag);
  }
}

void BasicCache::drain(std::vector<FlushContext> &list) {
  panic_if(list.size() == 0, "Empty flush list.");

  // Sort
  std::sort(list.begin(), list.end(), FlushContext::compare);

  // Find consecutive group and submit
  auto begin = list.begin();
  auto prev = begin;
  auto iter = std::next(begin);

  for (; iter < list.end(); iter++) {
    if (prev->lpn + 1 != iter->lpn) {
      drainRange(begin, iter);

      begin = iter;
    }

    prev++;
  }

  drainRange(begin, list.end());
}

void BasicCache::drainRange(std::vector<FlushContext>::iterator begin,
                            std::vector<FlushContext>::iterator end) {
  uint32_t nlp = std::distance(begin, end);

  for (auto iter = begin; iter < end; begin++) {
    uint64_t tag = ++drainCounter;

    drainQueue.emplace(tag, *iter);

    auto req = FTL::Request(FTL::Operation::Write, iter->lpn, iter->offset,
                            iter->length, begin->lpn, nlp, eventDrainDone, tag);

    req.setDRAMAddress(iter->address);
  }

  drained += nlp;
}

void BasicCache::drainDone(uint64_t tag) {
  auto iter = drainQueue.find(tag);

  panic_if(iter == drainQueue.end(), "Unexpected drain ID %" PRIu64 ".", tag);

  cache->nvmDone(iter->second.lpn);

  drainQueue.erase(iter);
}

void BasicCache::getStatList(std::vector<Stat> &list,
                             std::string prefix) noexcept {
  list.emplace_back(prefix + "prefetched", "Prefetched pages.");
  list.emplace_back(prefix + "drained", "Written pages.");
}

void BasicCache::getStatValues(std::vector<double> &values) noexcept {
  values.emplace_back((double)prefetched);
  values.emplace_back((double)drained);
}

void BasicCache::resetStatValues() noexcept {
  prefetched = 0;
  drained = 0;
}

void BasicCache::createCheckpoint(std::ostream &out) const noexcept {
  bool exist = detector != nullptr;

  BACKUP_SCALAR(out, exist);

  if (exist) {
    detector->createCheckpoint(out);
  }

  BACKUP_SCALAR(out, prefetchTrigger);
  BACKUP_SCALAR(out, lastPrefetched);

  BACKUP_SCALAR(out, drainCounter);

  uint64_t size = drainQueue.size();

  BACKUP_SCALAR(out, size);

  for (auto &iter : drainQueue) {
    BACKUP_SCALAR(out, iter.first);
    BACKUP_SCALAR(out, iter.second.lpn);
    BACKUP_SCALAR(out, iter.second.address);
    BACKUP_SCALAR(out, iter.second.offset);
    BACKUP_SCALAR(out, iter.second.length);
  }

  BACKUP_EVENT(out, eventDrainDone);
}

void BasicCache::restoreCheckpoint(std::istream &in) noexcept {
  bool exist;

  RESTORE_SCALAR(in, exist);

  panic_if((exist && !detector) || (!exist && detector),
           "Existance of sequential detector not matched.");

  RESTORE_SCALAR(in, prefetchTrigger)
  RESTORE_SCALAR(in, lastPrefetched);

  RESTORE_SCALAR(in, drainCounter);

  uint64_t size;
  uint64_t tag;
  FlushContext ctx(0, 0);

  RESTORE_SCALAR(in, size);

  for (uint64_t i = 0; i < size; i++) {
    RESTORE_SCALAR(in, tag);
    RESTORE_SCALAR(in, ctx.lpn);
    RESTORE_SCALAR(in, ctx.address);
    RESTORE_SCALAR(in, ctx.offset);
    RESTORE_SCALAR(in, ctx.length);

    drainQueue.emplace(tag, ctx);
  }
}

}  // namespace SimpleSSD::ICL
