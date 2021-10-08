// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 *         Junhyeok Jang <jhjang@camelab.org>
 */

#include "ftl/gc/naive.hh"

#include "ftl/allocator/abstract_allocator.hh"
#include "ftl/base/abstract_ftl.hh"
#include "ftl/mapping/abstract_mapping.hh"

namespace SimpleSSD::FTL::GC {

NaiveGC::NaiveGC(ObjectData &o, FTLObjectData &fo, FIL::FIL *f)
    : AbstractGC(o, fo, f),
      beginAt(0),
      firstRequestArrival(std::numeric_limits<uint64_t>::max()) {
  eventTrigger = createEvent([this](uint64_t, uint64_t) { trigger(); },
                             "FTL::GC::eventTrigger");

  resetStatValues();
}

NaiveGC::~NaiveGC() {}

uint32_t NaiveGC::getParallelBlockCount() {
  auto nandConfig = object.config->getNANDStructure();

  // Check parallel block erase
  auto superpageMask = (uint8_t)readConfigUint(
      Section::FlashTranslation, Config::Key::SuperpageAllocation);
  auto fgcErase = readConfigUint(Section::FlashTranslation,
                                 Config::Key::ForegroundBlockEraseLevel);
  auto bgcErase = readConfigUint(Section::FlashTranslation,
                                 Config::Key::BackgroundBlockEraseLevel);

  fgcBlocksToErase = 1;
  bgcBlocksToErase = 1;

  for (uint8_t i = 0; i < 4; i++) {
    if (!(superpageMask & nandConfig->pageAllocation[i])) {
      if (i < fgcErase) {
        fgcBlocksToErase *= param->parallelismLevel[i];
      }
      if (i < bgcErase) {
        bgcBlocksToErase *= param->parallelismLevel[i];
      }
    }
  }

  return MAX(fgcBlocksToErase, bgcBlocksToErase);
}

void NaiveGC::triggerForeground() {
  if (UNLIKELY(ftlobject.pAllocator->checkForegroundGCThreshold() &&
               state == State::Idle)) {
    state = State::Foreground;
    beginAt = getTick();

    scheduleNow(eventTrigger);
  }
}

void NaiveGC::requestArrived(Request *) {
  // Save tick for penalty calculation
  if (UNLIKELY(state >= State::Foreground)) {
    // GC in-progress
    firstRequestArrival = MIN(firstRequestArrival, getTick());
    stat.affected_requests++;
  }
}

void NaiveGC::trigger() {
  stat.fgcCount++;

  // Get blocks to erase
  for (uint32_t idx = 0; idx < fgcBlocksToErase; idx++) {
    ftlobject.pAllocator->getVictimBlock(targetBlocks[idx], method,
                                         eventReadPage, idx);
  }

  debugprint(logid, "GC    | Foreground | %u blocks", fgcBlocksToErase);
}

void NaiveGC::readPage(uint64_t now, uint32_t idx) {
  auto &targetBlock = targetBlocks[idx];

  if (LIKELY(targetBlock.pageReadIndex < targetBlock.copyList.size())) {
    stat.gcCopiedPages += superpage;
  }
  else {
    stat.gcErasedBlocks += superpage;
  }

  AbstractBlockCopyJob::readPage(now, idx);
}

void NaiveGC::done(uint64_t now, uint32_t idx) {
  auto &targetBlock = targetBlocks[idx];

  targetBlock.blockID.invalidate();

  // Check all GC has been completed
  checkDone(now);
}

void NaiveGC::checkDone(uint64_t now) {
  bool allinvalid = true;

  for (auto &iter : targetBlocks) {
    if (iter.blockID.isValid()) {
      allinvalid = false;
      break;
    }
  }

  if (allinvalid) {
    // Triggered GC completed
    if (state == State::Foreground) {
      debugprint(logid,
                 "GC    | Foreground | %" PRIu64 " - %" PRIu64 " (%" PRIu64 ")",
                 beginAt, now, now - beginAt);
    }
    else if (state == State::Background) {
      debugprint(logid,
                 "GC    | Background | %" PRIu64 " - %" PRIu64 " (%" PRIu64 ")",
                 beginAt, now, now - beginAt);
    }

    state = State::Idle;

    // Check threshold
    triggerForeground();

    // Calculate penalty
    updatePenalty(now);

    // As we got new freeblock, restart `some of` stalled requests
    ftlobject.pFTL->restartStalledRequests();
  }
}

void NaiveGC::getStatList(std::vector<Stat> &list,
                          std::string prefix) noexcept {
  list.emplace_back(prefix + "gc.foreground", "Total Foreground GC count");
  list.emplace_back(prefix + "gc.background", "Total Background GC count");
  list.emplace_back(prefix + "gc.block", "Total reclaimed blocks in GC");
  list.emplace_back(prefix + "gc.copy", "Total valid page copy");
  list.emplace_back(prefix + "gc.penalty.average", "Averagy penalty / GC");
  list.emplace_back(prefix + "gc.penalty.min", "Minimum penalty");
  list.emplace_back(prefix + "gc.penalty.max", "Maximum penalty");
  list.emplace_back(prefix + "gc.penalty.count", "# penalty calculation");
}

void NaiveGC::getStatValues(std::vector<double> &values) noexcept {
  auto copy = stat;

  if (firstRequestArrival != std::numeric_limits<uint64_t>::max()) {
    // On-going GC
    auto penalty = getTick() - firstRequestArrival;

    copy.penalty_count++;
    copy.avg_penalty += penalty;
    copy.min_penalty = MIN(copy.min_penalty, penalty);
    copy.max_penalty = MAX(copy.max_penalty, penalty);
  }

  values.push_back((double)copy.fgcCount);
  values.push_back((double)copy.bgcCount);
  values.push_back((double)copy.gcErasedBlocks);
  values.push_back((double)copy.gcCopiedPages);
  values.push_back(copy.penalty_count > 0
                       ? (double)copy.avg_penalty / copy.penalty_count
                       : 0.);
  values.push_back((double)(copy.penalty_count > 0 ? copy.min_penalty : 0));
  values.push_back((double)copy.max_penalty);
  values.push_back((double)copy.penalty_count);
}

void NaiveGC::resetStatValues() noexcept {
  memset(&stat, 0, sizeof(stat));

  stat.min_penalty = std::numeric_limits<uint64_t>::max();
}

void NaiveGC::createCheckpoint(std::ostream &out) const noexcept {
  AbstractGC::createCheckpoint(out);

  BACKUP_SCALAR(out, beginAt);
  BACKUP_SCALAR(out, fgcBlocksToErase);
  BACKUP_SCALAR(out, bgcBlocksToErase);

  BACKUP_SCALAR(out, stat);
  BACKUP_SCALAR(out, firstRequestArrival);

  BACKUP_EVENT(out, eventTrigger);
}

void NaiveGC::restoreCheckpoint(std::istream &in) noexcept {
  AbstractGC::restoreCheckpoint(in);

  RESTORE_SCALAR(in, beginAt);
  RESTORE_SCALAR(in, fgcBlocksToErase);
  RESTORE_SCALAR(in, bgcBlocksToErase);

  RESTORE_SCALAR(in, stat);
  RESTORE_SCALAR(in, firstRequestArrival);

  RESTORE_EVENT(in, eventTrigger);
}

}  // namespace SimpleSSD::FTL::GC
