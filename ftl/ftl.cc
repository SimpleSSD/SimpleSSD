// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 *         Junhyeok Jang <jhjang@camelab.org>
 */

#include "ftl/ftl.hh"

#include "ftl/allocator/generic_allocator.hh"
#include "ftl/allocator/victim_selection.hh"
#include "ftl/base/page_level_ftl.hh"
#include "ftl/filling.hh"
#include "ftl/gc/advanced.hh"
#include "ftl/gc/naive.hh"
#include "ftl/gc/preemption.hh"
#include "ftl/mapping/page_level_mapping.hh"
#include "icl/icl.hh"

namespace SimpleSSD::FTL {

FTL::FTL(ObjectData &o)
    : Object(o), jobManager(o), ftlobject(jobManager), requestCounter(0) {
  pFIL = new FIL::FIL(object);

  auto mapping = (Config::MappingType)readConfigUint(Section::FlashTranslation,
                                                     Config::Key::MappingMode);
  auto gcmode = (Config::GCType)readConfigUint(Section::FlashTranslation,
                                               Config::Key::GCMode);

  // Mapping algorithm
  switch (mapping) {
    case Config::MappingType::PageLevelFTL:
      ftlobject.pMapping = new Mapping::PageLevelMapping(object, ftlobject);

      break;
    // case Config::MappingType::BlockLevelFTL:
    //   ftlobject.pMapping = new Mapping::BlockLevel(object);

    //   break;
    default:
      panic("Unsupported mapping type.");

      break;
  }

  // Block allocator
  switch (mapping) {
    default:
      ftlobject.pAllocator =
          new BlockAllocator::GenericAllocator(object, ftlobject);

      break;
  }

  // Initialize algorithms before creating background jobs
  BlockAllocator::initializeVictimSelectionAlgorithms(object,
                                                      ftlobject.pAllocator);

  // GC algorithm
  AbstractJob *gcjob = nullptr;

  switch (gcmode) {
    case Config::GCType::Naive:
      gcjob = new GC::NaiveGC(object, ftlobject, pFIL);

      break;
    case Config::GCType::Advanced:
      gcjob = new GC::AdvancedGC(object, ftlobject, pFIL);

      break;
    case Config::GCType::Preemptible:
      gcjob = new GC::PreemptibleGC(object, ftlobject, pFIL);

      break;
    default:
      panic("Unsupported GC type.");

      break;
  }

  jobManager.addJob(gcjob);

  // Base FTL routine
  switch (mapping) {
    case Config::MappingType::PageLevelFTL:
      ftlobject.pFTL = new PageLevelFTL(object, ftlobject, this, pFIL);

      break;
    default:
      panic("Unsupported mapping type.");

      break;
  }
}

FTL::~FTL() {
  delete pFIL;
  delete ftlobject.pMapping;
  delete ftlobject.pAllocator;
  delete ftlobject.pFTL;

  BlockAllocator::finalizeVictimSelectionAlgorithms();
}

Request *FTL::insertRequest(Request &&req) {
  auto ret = requestQueue.emplace(++requestCounter, std::move(req));

  panic_if(!ret.second, "Duplicated request ID.");

  ret.first->second.tag = requestCounter;

  return &ret.first->second;
}

void FTL::initialize() {
  // Initialize all
  ftlobject.pAllocator->initialize();
  ftlobject.pMapping->initialize();

  jobManager.initialize();

  ftlobject.pFTL->initialize();

  if (UNLIKELY(
          readConfigBoolean(Section::Simulation,
                            SimpleSSD::Config::Key::RestoreFromCheckpoint))) {
    debugprint(Log::DebugID::FTL, "Restoring from checkpoint. Skip filling.");
  }
  else {
    // Filling
    Filling filler(object, ftlobject);

    filler.start();
  }
}

const Parameter *FTL::getInfo() {
  return ftlobject.pMapping->getInfo();
}

uint64_t FTL::getPageUsage(LPN slpn, uint64_t nlp) {
  return ftlobject.pMapping->getPageUsage(slpn, nlp);
}

Request *FTL::getRequest(uint64_t tag) {
  auto iter = requestQueue.find(tag);

  panic_if(iter == requestQueue.end(), "Unexpected command tag %" PRIu64 ".",
           tag);

  return &iter->second;
}

void FTL::completeRequest(Request *req) {
  scheduleNow(req->event, req->data);

  auto iter = requestQueue.find(req->tag);

  requestQueue.erase(iter);
}

void FTL::read(Request &&req) {
  auto preq = insertRequest(std::move(req));

  debugprint(Log::DebugID::FTL, "READ  | LPN %" PRIu64, preq->lpn);

  ftlobject.pFTL->read(preq);
}

void FTL::write(Request &&req) {
  auto preq = insertRequest(std::move(req));

  debugprint(Log::DebugID::FTL, "WRITE | LPN %" PRIu64, preq->lpn);

  ftlobject.pFTL->write(preq);
}

void FTL::invalidate(Request &&req) {
  auto preq = insertRequest(std::move(req));

  debugprint(Log::DebugID::FTL, "INVAL | LPN %" PRIu64, preq->lpn);

  ftlobject.pFTL->invalidate(preq);
}

void FTL::getStatList(std::vector<Stat> &list, std::string prefix) noexcept {
  jobManager.getStatList(list, prefix + "ftl.job.");
  ftlobject.pFTL->getStatList(list, prefix + "ftl.base.");
  ftlobject.pMapping->getStatList(list, prefix + "ftl.mapper.");
  ftlobject.pAllocator->getStatList(list, prefix + "ftl.allocator.");
  pFIL->getStatList(list, prefix);
}

void FTL::getStatValues(std::vector<double> &values) noexcept {
  jobManager.getStatValues(values);
  ftlobject.pFTL->getStatValues(values);
  ftlobject.pMapping->getStatValues(values);
  ftlobject.pAllocator->getStatValues(values);
  pFIL->getStatValues(values);
}

void FTL::resetStatValues() noexcept {
  jobManager.resetStatValues();
  ftlobject.pFTL->resetStatValues();
  ftlobject.pMapping->resetStatValues();
  ftlobject.pAllocator->resetStatValues();
  pFIL->resetStatValues();
}

void FTL::createCheckpoint(std::ostream &out) const noexcept {
  BACKUP_SCALAR(out, requestCounter);

  BACKUP_STL(out, requestQueue, iter, {
    BACKUP_SCALAR(out, iter.first);

    iter.second.createCheckpoint(out);
  });

  jobManager.createCheckpoint(out);
  ftlobject.pFTL->createCheckpoint(out);
  ftlobject.pMapping->createCheckpoint(out);
  ftlobject.pAllocator->createCheckpoint(out);
  pFIL->createCheckpoint(out);
}

void FTL::restoreCheckpoint(std::istream &in) noexcept {
  RESTORE_SCALAR(in, requestCounter);

  RESTORE_STL_RESERVE(in, requestQueue, i, {
    uint64_t tag;

    RESTORE_SCALAR(in, tag);

    auto iter = requestQueue.emplace(tag, Request{PPN{}});

    iter.first->second.restoreCheckpoint(in, object);
  });

  jobManager.restoreCheckpoint(in);
  ftlobject.pFTL->restoreCheckpoint(in);
  ftlobject.pMapping->restoreCheckpoint(in);
  ftlobject.pAllocator->restoreCheckpoint(in);
  pFIL->restoreCheckpoint(in);
}

}  // namespace SimpleSSD::FTL
