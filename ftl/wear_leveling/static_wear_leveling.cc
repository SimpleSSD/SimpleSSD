// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "ftl/wear_leveling/static_wear_leveling.hh"

#include "ftl/allocator/abstract_allocator.hh"
#include "ftl/allocator/victim_selection.hh"
#include "ftl/mapping/abstract_mapping.hh"

namespace SimpleSSD::FTL::WearLeveling {

StaticWearLeveling::StaticWearLeveling(ObjectData &o, FTLObjectData &fo,
                                       FIL::FIL *fil)
    : AbstractWearLeveling(o, fo, fil), beginAt(0) {
  method = BlockAllocator::getVictimSelectionAlgorithm(
      BlockAllocator::VictimSelectionID::LeastErased);

  resetStatValues();
}

void StaticWearLeveling::triggerForeground(uint64_t now) {
  auto &targetBlock = targetBlocks[0];

  // 1. Calculate wear leveling factor
  // 2. Check threshold
  // 3. If wear leveling factor <= threshold, do below
  if (state < State::Foreground) {
    ftlobject.pAllocator->getVictimBlock(targetBlock, method, eventReadPage, 0);

    state = State::Foreground;
    stat.foreground++;
    beginAt = now;
  }
}

StaticWearLeveling::~StaticWearLeveling() {}

void StaticWearLeveling::blockEraseCallback(uint64_t now, const PSBN &) {
  triggerForeground(now);
}

void StaticWearLeveling::readPage(uint64_t now, uint32_t) {
  auto &targetBlock = targetBlocks[0];

  if (LIKELY(targetBlock.pageReadIndex < targetBlock.copyList.size())) {
    stat.copiedPages += superpage;
  }
  else {
    stat.erasedBlocks += superpage;
  }

  AbstractBlockCopyJob::readPage(now, 0);
}

void StaticWearLeveling::updateMapping(uint64_t now, uint32_t) {
  auto &targetBlock = targetBlocks[0];

  targetBlock.readCounter--;

  if (targetBlock.readCounter == 0) {
    // Start address translation
    auto &ctx = targetBlock.copyList.at(targetBlock.pageWriteIndex);
    auto lpn = ctx.request.getLPN();
    auto ppn = ctx.request.getPPN();

    panic_if(!lpn.isValid(), "Invalid LPN received.");

    if (superpage > 1) {
      debugprint(
          logid,
          "%s| READ  | PSBN %" PRIx64 "h | PSPN %" PRIx64 "h -> LSPN %" PRIx64
          "h | %" PRIu64 " - %" PRIu64 " (%" PRIu64 ")",
          logprefix, targetBlock.blockID, param->getPSPNFromPPN(ppn),
          param->getLSPNFromLPN(lpn), ctx.beginAt, now, now - ctx.beginAt);
    }
    else {
      debugprint(logid,
                 "%s| READ  | PBN %" PRIx64 "h | PPN %" PRIx64
                 "h -> LPN %" PRIx64 "h | %" PRIu64 " - %" PRIu64 " (%" PRIu64
                 ")",
                 logprefix, targetBlock.blockID, ppn, lpn, ctx.beginAt, now,
                 now - ctx.beginAt);
    }

    ftlobject.pMapping->writeMapping(
        &ctx.request, eventWritePage, true,
        BlockAllocator::AllocationStrategy::HighestEraseCount);
  }
}

void StaticWearLeveling::done(uint64_t now, uint32_t) {
  auto &targetBlock = targetBlocks[0];

  targetBlock.blockID.invalidate();

  if (state == State::Foreground) {
    debugprint(logid,
               "WL    | Foreground | %" PRIu64 " - %" PRIu64 " (%" PRIu64 ")",
               beginAt, now, now - beginAt);
  }
  else if (state == State::Background) {
    debugprint(logid,
               "WL    | Background | %" PRIu64 " - %" PRIu64 " (%" PRIu64 ")",
               beginAt, now, now - beginAt);
  }

  state = State::Idle;

  triggerForeground(now);
}

void StaticWearLeveling::getStatList(std::vector<Stat> &list,
                                     std::string prefix) noexcept {
  list.emplace_back(prefix + "wear_leveling.foreground",
                    "Total wear leveling triggered in foreground");
  list.emplace_back(prefix + "wear_leveling.background",
                    "Total wear leveling triggered in background");
  list.emplace_back(prefix + "wear_leveling.block", "Total reclaimed blocks");
  list.emplace_back(prefix + "wear_leveling.copy", "Total valid page copy");
}

void StaticWearLeveling::getStatValues(std::vector<double> &values) noexcept {
  values.push_back((double)stat.foreground);
  values.push_back((double)stat.background);
  values.push_back((double)stat.erasedBlocks);
  values.push_back((double)stat.copiedPages);
}

void StaticWearLeveling::resetStatValues() noexcept {
  stat.foreground = 0;
  stat.background = 0;
  stat.copiedPages = 0;
  stat.erasedBlocks = 0;
}

void StaticWearLeveling::createCheckpoint(std::ostream &out) const noexcept {
  AbstractWearLeveling::createCheckpoint(out);

  BACKUP_SCALAR(out, beginAt);
  BACKUP_SCALAR(out, stat);
}

void StaticWearLeveling::restoreCheckpoint(std::istream &in) noexcept {
  AbstractWearLeveling::restoreCheckpoint(in);

  RESTORE_SCALAR(in, beginAt);
  RESTORE_SCALAR(in, stat);
}

}  // namespace SimpleSSD::FTL::WearLeveling
