// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "icl/icl.hh"

#include "hil/hil.hh"
#include "icl/cache/generic_cache.hh"
#include "icl/manager/basic.hh"
#include "util/algorithm.hh"

namespace SimpleSSD::ICL {

ICL::ICL(ObjectData &o, HIL::HIL *p) : Object(o), pHIL(p) {
  pFTL = new FTL::FTL(object);
  auto *param = pFTL->getInfo();

  totalLogicalPages = param->totalLogicalPages;
  logicalPageSize = param->pageSize;

  // Create cache manager
  auto mode = (Config::Mode)readConfigUint(Section::InternalCache,
                                           Config::Key::CacheMode);

  switch (mode) {
    case Config::Mode::None:
    case Config::Mode::SetAssociative:
    case Config::Mode::RingBuffer:
      pManager = new Manager::BasicManager(object, this, pFTL);

      break;
    default:
      panic("Unexpected internal cache model.");

      break;
  }

  // Create cache structure
  switch (mode) {
    case Config::Mode::None:
      // Create very small - two superpage size - ring buffer.
      writeConfigUint(Section::InternalCache, Config::Key::CacheSize,
                      param->parallelismLevel[0] * param->pageSize * 2);

      /* fallthrough */
    case Config::Mode::SetAssociative:
    case Config::Mode::RingBuffer:
      pCache = new Cache::GenericCache(object, pManager, param);

      break;
    default:
      panic("Unexpected internal cache model.");

      break;
  }

  // Initialize
  pManager->initialize(pCache);

  // Must be called later -- because of memory allocation
  pFTL->initialize();

  // Create events
  eventPrefetch = createEvent(
      [this](uint64_t, uint64_t) {
        while (prefetchQueue.size() > 0) {
          auto &front = prefetchQueue.front();
          auto *req = new HIL::Request(InvalidEventID, 0);

          req->setAddress(front.first, front.second, logicalPageSize);

          pHIL->read(req);

          prefetchQueue.pop_front();
        }
      },
      "ICL::eventPrefetch");
}

ICL::~ICL() {
  delete pManager;
  delete pCache;
  delete pFTL;
}

void ICL::setCallbackFunction(Event e) {
  pManager->setCallbackFunction(e);
}

void ICL::read(HIL::SubRequest *req) {
  pManager->read(req);
}

void ICL::write(HIL::SubRequest *req) {
  pManager->write(req);
}

void ICL::flush(HIL::SubRequest *req) {
  pManager->flush(req);
}

void ICL::format(HIL::SubRequest *req) {
  pManager->erase(req);
}

void ICL::done(HIL::SubRequest *req) {
  pManager->dmaDone(req);
}

void ICL::makeRequest(LPN slpn, uint32_t length) {
  prefetchQueue.emplace_back(slpn, length);

  if (!isScheduled(eventPrefetch)) {
    scheduleNow(eventPrefetch);
  }
}

LPN ICL::getPageUsage(LPN offset, LPN length) {
  return pFTL->getPageUsage(offset, length);
}

LPN ICL::getTotalPages() {
  return totalLogicalPages;
}

uint32_t ICL::getLPNSize() {
  return logicalPageSize;
}

HIL::SubRequest *ICL::getSubRequest(uint64_t tag) {
  return pHIL->getSubRequest(tag);
}

void ICL::getStatList(std::vector<Stat> &list, std::string prefix) noexcept {
  pCache->getStatList(list, prefix + "icl.cache.");
  pManager->getStatList(list, prefix + "icl.manager.");
  pFTL->getStatList(list, prefix);
}

void ICL::getStatValues(std::vector<double> &values) noexcept {
  pCache->getStatValues(values);
  pManager->getStatValues(values);
  pFTL->getStatValues(values);
}

void ICL::resetStatValues() noexcept {
  pCache->resetStatValues();
  pManager->resetStatValues();
  pFTL->resetStatValues();
}

void ICL::createCheckpoint(std::ostream &out) const noexcept {
  BACKUP_SCALAR(out, totalLogicalPages);
  BACKUP_SCALAR(out, logicalPageSize);

  uint64_t size = prefetchQueue.size();
  BACKUP_SCALAR(out, size);

  for (auto &iter : prefetchQueue) {
    BACKUP_SCALAR(out, iter.first);
    BACKUP_SCALAR(out, iter.second);
  }

  BACKUP_EVENT(out, eventPrefetch)

  pCache->createCheckpoint(out);
  pManager->createCheckpoint(out);
  pFTL->createCheckpoint(out);
}

void ICL::restoreCheckpoint(std::istream &in) noexcept {
  RESTORE_SCALAR(in, totalLogicalPages);
  RESTORE_SCALAR(in, logicalPageSize);

  uint64_t size;
  RESTORE_SCALAR(in, size);

  for (uint64_t i = 0; i < size; i++) {
    LPN l;
    uint32_t s;

    RESTORE_SCALAR(in, l);
    RESTORE_SCALAR(in, s);

    prefetchQueue.emplace_back(l, s);
  }

  RESTORE_EVENT(in, eventPrefetch);

  pCache->restoreCheckpoint(in);
  pManager->restoreCheckpoint(in);
  pFTL->restoreCheckpoint(in);
}

HIL::SubRequest *ICL::restoreSubRequest(uint64_t tag) noexcept {
  return pHIL->restoreSubRequest(tag);
}

}  // namespace SimpleSSD::ICL
