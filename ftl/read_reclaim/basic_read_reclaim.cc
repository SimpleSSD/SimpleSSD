// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "ftl/read_reclaim/basic_read_reclaim.hh"

#include "ftl/allocator/abstract_allocator.hh"

namespace SimpleSSD::FTL::ReadReclaim {

BasicReadReclaim::BasicReadReclaim(ObjectData &o, FTLObjectData &fo,
                                   FIL::FIL *fil)
    : AbstractReadReclaim(o, fo, fil), beginAt(0) {
  resetStatValues();
}

BasicReadReclaim::~BasicReadReclaim() {}

void BasicReadReclaim::readPage(uint64_t now, uint32_t) {
  auto &targetBlock = targetBlocks[0];

  if (LIKELY(targetBlock.pageReadIndex < targetBlock.copyList.size())) {
    stat.copiedPages += superpage;
  }
  else {
    stat.erasedBlocks += superpage;
  }

  AbstractBlockCopyJob::readPage(now, 0);
}

void BasicReadReclaim::done(uint64_t now, uint32_t) {
  auto &targetBlock = targetBlocks[0];

  targetBlock.blockID.invalidate();

  if (state == State::Foreground) {
    debugprint(logid,
               "RR    | Foreground | %" PRIu64 " - %" PRIu64 " (%" PRIu64 ")",
               beginAt, now, now - beginAt);
  }
  else if (state == State::Background) {
    debugprint(logid,
               "RR    | Background | %" PRIu64 " - %" PRIu64 " (%" PRIu64 ")",
               beginAt, now, now - beginAt);
  }

  if (UNLIKELY(pendingList.size() > 0)) {
    targetBlock.blockID = pendingList.front().second;

    pendingList.pop_front();

    ftlobject.pAllocator->getVictimBlock(targetBlock, nullptr, eventReadPage,
                                         0);
  }
  else {
    state = State::Idle;
  }
}

void BasicReadReclaim::initialize() {
  configure(Log::DebugID::FTL_BasicReadReclaim, "RR    ", "FTL::ReadReclaim",
            1);
}

bool BasicReadReclaim::doErrorCheck(const PPN &ppn) {
  uint64_t now = getTick();
  auto &targetBlock = targetBlocks[0];

  auto pspn = param->getPSPNFromPPN(ppn);
  auto psbn = param->getPSBNFromPSPN(pspn);
  auto nbiterr = estimateBitError(now, psbn);

  // TODO: hard-coded
  if (nbiterr >= 50) {
    if (state < State::Foreground) {
      targetBlock.blockID = psbn;

      ftlobject.pAllocator->getVictimBlock(targetBlock, nullptr, eventReadPage,
                                           0);

      state = State::Foreground;
      stat.foreground++;
      beginAt = now;
    }
    else {
      // Read reclaim in progress
      if (LIKELY(targetBlock.blockID != psbn)) {
        pendingList.push_back(psbn, psbn);
      }
    }

    return true;
  }

  return false;
}

void BasicReadReclaim::getStatList(std::vector<Stat> &list,
                                   std::string prefix) noexcept {
  list.emplace_back(prefix + "read_reclaim.foreground",
                    "Total read reclaim triggered in foreground");
  list.emplace_back(prefix + "read_reclaim.background",
                    "Total read reclaim triggered in background");
  list.emplace_back(prefix + "read_reclaim.block", "Total reclaimed blocks");
  list.emplace_back(prefix + "read_reclaim.copy", "Total valid page copy");
}

void BasicReadReclaim::getStatValues(std::vector<double> &values) noexcept {
  values.push_back((double)stat.foreground);
  values.push_back((double)stat.background);
  values.push_back((double)stat.copiedPages);
  values.push_back((double)stat.erasedBlocks);
}

void BasicReadReclaim::resetStatValues() noexcept {
  stat.foreground = 0;
  stat.background = 0;
  stat.copiedPages = 0;
  stat.erasedBlocks = 0;
}

void BasicReadReclaim::createCheckpoint(std::ostream &out) const noexcept {
  AbstractReadReclaim::createCheckpoint(out);

  BACKUP_SCALAR(out, beginAt);
  BACKUP_SCALAR(out, stat);

  BACKUP_STL(out, pendingList, iter, BACKUP_SCALAR(out, iter.second));
}

void BasicReadReclaim::restoreCheckpoint(std::istream &in) noexcept {
  AbstractReadReclaim::restoreCheckpoint(in);

  RESTORE_SCALAR(in, beginAt);
  RESTORE_SCALAR(in, stat);

  RESTORE_STL_RESERVE(in, pendingList, iter, {
    PSBN psbn;

    RESTORE_SCALAR(in, psbn);

    pendingList.push_back(psbn, psbn);
  });
}

}  // namespace SimpleSSD::FTL::ReadReclaim
