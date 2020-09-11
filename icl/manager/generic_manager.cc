// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 *         Junhyeok Jang <jhjang@camelab.org>
 */

#include "icl/manager/generic_manager.hh"

#include "icl/cache/abstract_cache.hh"

namespace SimpleSSD::ICL::Manager {

GenericDetector::GenericDetector(uint32_t p, uint64_t c, uint64_t r)
    : SequentialDetector(p),
      lastRequestTag(1),
      offset(std::numeric_limits<uint64_t>::max()),
      length(0),
      reqLength(0),
      hitCounter(0),
      accessCounter(0),
      triggerCount(c),
      triggerRatio(r) {}

void GenericDetector::submitSubRequest(HIL::SubRequest *req) {
  uint64_t tag = req->getParentTag();
  auto lpn = req->getLPN();

  if (lastRequestTag != tag) {
    if (offset + length == lpn * pageSize + req->getSkipFront()) {
      if (!enabled) {
        hitCounter++;
        accessCounter += reqLength;

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

    reqLength = 0;
    lastRequestTag = tag;
  }

  offset = lpn * pageSize + req->getSkipFront();
  length = req->getLength();
  reqLength += length;
}

void GenericDetector::createCheckpoint(std::ostream &out) const noexcept {
  SequentialDetector::createCheckpoint(out);

  BACKUP_SCALAR(out, lastRequestTag);
  BACKUP_SCALAR(out, offset);
  BACKUP_SCALAR(out, length);
  BACKUP_SCALAR(out, hitCounter);
  BACKUP_SCALAR(out, accessCounter);
}

void GenericDetector::restoreCheckpoint(std::istream &in,
                                        ObjectData &object) noexcept {
  SequentialDetector::restoreCheckpoint(in, object);

  RESTORE_SCALAR(in, lastRequestTag);
  RESTORE_SCALAR(in, offset);
  RESTORE_SCALAR(in, length);
  RESTORE_SCALAR(in, hitCounter);
  RESTORE_SCALAR(in, accessCounter);
}

GenericManager::GenericManager(ObjectData &o, ICL *p, FTL::FTL *f)
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

    detector = new GenericDetector(ftlparam->pageSize, count, ratio);
  }

  auto prefetchMode = (Config::Granularity)readConfigUint(
      Section::InternalCache, Config::Key::PrefetchMode);

  prefetchTrigger = std::numeric_limits<LPN>::max();
  lastPrefetched = static_cast<LPN>(0);

  prefetchCount = 1;

  if (prefetchMode >= Config::Granularity::FirstLevel) {
    prefetchCount *= ftlparam->parallelismLevel[0];
  }
  if (prefetchMode >= Config::Granularity::SecondLevel) {
    prefetchCount *= ftlparam->parallelismLevel[1];
  }
  if (prefetchMode >= Config::Granularity::ThirdLevel) {
    prefetchCount *= ftlparam->parallelismLevel[2];
  }
  if (prefetchMode >= Config::Granularity::AllLevel) {
    prefetchCount *= ftlparam->parallelismLevel[3];
  }

  memset(&stat, 0, sizeof(stat));

  eventDrainDone =
      createEvent([this](uint64_t t, uint64_t d) { drainDone(t, d); },
                  "ICL::BasicManager::eventDrainDone");
  eventReadDone = createEvent([this](uint64_t, uint64_t d) { readDone(d); },
                              "ICL::BasicManager::eventReadDone");
}

GenericManager::~GenericManager() {
  if (detector) {
    delete detector;
  }
}

void GenericManager::read(HIL::SubRequest *req) {
  cache->lookup(req);

  if (detector && !req->isICLRequest()) {
    bool old = detector->isEnabled();

    detector->submitSubRequest(req);

    if (detector->isEnabled()) {
      LPN nextlpn = static_cast<LPN>(req->getSLPN() + req->getNLP());

      if (old) {
        // Continued - prefetch
        if (nextlpn < prefetchTrigger) {
          // Not yet
          return;
        }

        nextlpn = lastPrefetched;

        debugprint_basic(req, "PREFETCH");
      }
      else {
        // First - read-ahead
        debugprint_basic(req, "READ-AHEAD");
      }

      // Make prefetch request
      LPN begin = nextlpn;

      prefetchTrigger = static_cast<LPN>(begin + prefetchCount / 2);
      lastPrefetched = static_cast<LPN>(begin + prefetchCount);

      stat.prefetched += prefetchCount;

      pICL->makeRequest(begin, prefetchCount);
    }
  }
}

void GenericManager::write(HIL::SubRequest *req) {
  cache->lookup(req);
}

void GenericManager::flush(HIL::SubRequest *req) {
  cache->flush(req);
}

void GenericManager::erase(HIL::SubRequest *req) {
  cache->erase(req);
}

void GenericManager::dmaDone(HIL::SubRequest *req) {
  cache->dmaDone(req->getLPN());
}

void GenericManager::lookupDone(uint64_t tag) {
  auto req = getSubRequest(tag);

  if (req->getMiss()) {
    stat.miss++;
    debugprint_basic(req, "CACHE MISS");
  }
  else {
    stat.hit++;
    debugprint_basic(req, "CACHE HIT");
  }

  if (req->getAllocate()) {
    // We need allocation
    cache->allocate(req);
  }
  else {
    cacheDone(tag);
  }
}

void GenericManager::cacheDone(uint64_t tag) {
  auto req = getSubRequest(tag);
  auto opcode = req->getOpcode();

  // Submit to FIL
  if (opcode == HIL::Operation::Read && req->getMiss()) {
    pFTL->read(FTL::Request(eventReadDone, req));
  }
  else if (opcode == HIL::Operation::Trim || opcode == HIL::Operation::Format) {
    pFTL->invalidate(FTL::Request(eventICLCompletion, req));
  }
  else {
    scheduleNow(eventICLCompletion, tag);
  }
}

uint64_t GenericManager::drain(std::vector<FlushContext> &list) {
  uint64_t now = getTick();
  uint64_t size = list.size();

  panic_if(size == 0, "Empty flush list.");

  debugprint(Log::DebugID::ICL_BasicManager, "DRAIN | %" PRIu64 " PAGES", size);

  // Sort
  std::sort(list.begin(), list.end(), FlushContext::compare);

  // Find consecutive group and submit
  auto begin = list.begin();
  auto prev = begin;
  auto iter = std::next(begin);

  begin->flushedAt = now;

  for (; iter < list.end(); iter++) {
    iter->flushedAt = now;

    if (prev->lpn + 1 != iter->lpn) {
      drainRange(begin, iter);

      begin = iter;
    }

    prev++;
  }

  drainRange(begin, list.end());

  stat.eviction++;

  return drainCounter;
}

void GenericManager::drainRange(std::vector<FlushContext>::iterator begin,
                                std::vector<FlushContext>::iterator end) {
  uint32_t nlp = std::distance(begin, end);

  debugprint(Log::DebugID::ICL_BasicManager, "DRAIN | LPN %" PRIu64 " + %u",
             begin->lpn, nlp);

  for (auto iter = begin; iter < end; iter++) {
    uint64_t tag = ++drainCounter;

    drainQueue.emplace(tag, *iter);

    auto req = FTL::Request(FTL::Operation::Write, iter->lpn, iter->offset,
                            iter->length, begin->lpn, nlp, eventDrainDone, tag);

    req.setDRAMAddress(iter->address);

    pFTL->write(std::move(req));
  }

  stat.drained += nlp;
}

void GenericManager::drainDone(uint64_t now, uint64_t tag) {
  auto iter = drainQueue.find(tag);

  panic_if(iter == drainQueue.end(), "Unexpected drain ID %" PRIu64 ".", tag);

  debugprint(Log::DebugID::ICL_BasicManager,
             "DRAIN | LPN %" PRIu64 " | %" PRIu64 " - %" PRIu64 " (%" PRIu64
             ")",
             iter->second.lpn, iter->second.flushedAt, now,
             now - iter->second.flushedAt);

  cache->nvmDone(iter->second.lpn, tag, true);

  drainQueue.erase(iter);
}

void GenericManager::readDone(uint64_t tag) {
  auto req = getSubRequest(tag);

  cache->nvmDone(req->getLPN(), tag, false);

  scheduleNow(eventICLCompletion, tag);
}

void GenericManager::getStatList(std::vector<Stat> &list,
                                 std::string prefix) noexcept {
  list.emplace_back(prefix + "prefetched", "Prefetched pages");
  list.emplace_back(prefix + "drained", "Written pages");
  list.emplace_back(prefix + "hit", "Number of cache hit");
  list.emplace_back(prefix + "miss", "Number of cache miss");
  list.emplace_back(prefix + "eviction", "Number of cache eviction");
}

void GenericManager::getStatValues(std::vector<double> &values) noexcept {
  values.emplace_back((double)stat.prefetched);
  values.emplace_back((double)stat.drained);
  values.emplace_back((double)stat.hit);
  values.emplace_back((double)stat.miss);
  values.emplace_back((double)stat.eviction);
}

void GenericManager::resetStatValues() noexcept {
  memset(&stat, 0, sizeof(stat));
}

void GenericManager::createCheckpoint(std::ostream &out) const noexcept {
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
  BACKUP_EVENT(out, eventReadDone);
}

void GenericManager::restoreCheckpoint(std::istream &in) noexcept {
  bool exist;

  RESTORE_SCALAR(in, exist);

  panic_if((exist && !detector) || (!exist && detector),
           "Existance of sequential detector not matched.");

  if (exist) {
    detector->restoreCheckpoint(in, object);
  }

  RESTORE_SCALAR(in, prefetchTrigger)
  RESTORE_SCALAR(in, lastPrefetched);

  RESTORE_SCALAR(in, drainCounter);

  uint64_t size;
  uint64_t tag;
  FlushContext ctx(static_cast<LPN>(0), 0);

  RESTORE_SCALAR(in, size);

  for (uint64_t i = 0; i < size; i++) {
    RESTORE_SCALAR(in, tag);
    RESTORE_SCALAR(in, ctx.lpn);
    RESTORE_SCALAR(in, ctx.address);
    RESTORE_SCALAR(in, ctx.offset);
    RESTORE_SCALAR(in, ctx.length);

    drainQueue.emplace(tag, ctx);
  }

  RESTORE_EVENT(in, eventDrainDone);
  RESTORE_EVENT(in, eventReadDone);
}

}  // namespace SimpleSSD::ICL::Manager
