// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "ftl/read_reclaim/basic_read_reclaim.hh"

#include "ftl/allocator/abstract_allocator.hh"
#include "ftl/mapping/abstract_mapping.hh"

namespace SimpleSSD::FTL::ReadReclaim {

BasicReadReclaim::BasicReadReclaim(ObjectData &o, FTLObjectData &fo,
                                   FIL::FIL *fil)
    : AbstractReadReclaim(o, fo, fil), beginAt(0) {
  logid = Log::DebugID::FTL_BasicReadReclaim;

  resetStatValues();

  superpage = param->superpage;

  auto memsize = superpage * pageSize;

  if (object.memory->allocate(memsize, Memory::MemoryType::SRAM, "", true) ==
      0) {
    bufferBaseAddress = object.memory->allocate(
        memsize, Memory::MemoryType::SRAM, "FTL::ReadReclaim::Buffer");
  }
  else {
    bufferBaseAddress = object.memory->allocate(
        memsize, Memory::MemoryType::DRAM, "FTL::ReadReclaim::Buffer");
  }

  eventDoRead = createEvent([this](uint64_t t, uint64_t) { read(t); },
                            "FTL::ReadReclaim::eventDoRead");
  eventDoTranslate = createEvent([this](uint64_t t, uint64_t) { translate(t); },
                                 "FTL::ReadReclaim::eventDoTranslate");
  eventDoWrite = createEvent([this](uint64_t t, uint64_t) { write(t); },
                             "FTL::ReadReclaim::eventDoWrite");
  eventWriteDone = createEvent([this](uint64_t t, uint64_t) { writeDone(t); },
                               "FTL::ReadReclaim::eventWriteDone");
  eventEraseDone = createEvent([this](uint64_t t, uint64_t) { eraseDone(t); },
                               "FTL::ReadReclaim::eventEraseDone");
  eventDone = createEvent([this](uint64_t t, uint64_t) { done(t); },
                          "FTL::ReadReclaim::eventDone");
}

BasicReadReclaim::~BasicReadReclaim() {}

void BasicReadReclaim::read(uint64_t now) {
  // Note: almost same with GC
  LPN invlpn;

  if (LIKELY(targetBlock.pageReadIndex < targetBlock.copyList.size())) {
    auto &ctx = targetBlock.copyList.at(targetBlock.pageReadIndex++);
    auto ppn = param->makePPN(targetBlock.blockID, 0, ctx.pageIndex);

    if (superpage > 1) {
      debugprint(logid, "RR    | READ  | PSBN %" PRIx64 "h | PSPN %" PRIx64 "h",
                 targetBlock.blockID, param->getPSPNFromPPN(ppn));
    }
    else {
      debugprint(logid, "RR    | READ  | PBN %" PRIx64 "h | PPN %" PRIx64 "h",
                 targetBlock.blockID, ppn);
    }

    for (uint32_t i = 0; i < superpage; i++) {
      ppn = param->makePPN(targetBlock.blockID, i, ctx.pageIndex);

      // Fill request
      if (i == 0) {
        ctx.request.setPPN(ppn);
        ctx.request.setDRAMAddress(makeBufferAddress(i));

        pFIL->read(FIL::Request(&ctx.request, eventDoTranslate));
      }
      else {
        pFIL->read(FIL::Request(invlpn, ppn, makeBufferAddress(i),
                                eventDoTranslate, 0));
      }
    }

    targetBlock.readCounter = superpage;
    ctx.beginAt = now;
    stat.copiedPages += superpage;
  }
  else {
    // Do erase
    PSBN psbn = targetBlock.blockID;

    // Erase
    if (superpage > 1) {
      debugprint(logid, "RR    | ERASE | PSBN %" PRIx64 "h", psbn);
    }
    else {
      // PSBN == PBN when superpage == 1
      debugprint(logid, "RR    | ERASE | PBN %" PRIx64 "h", psbn);
    }

    for (uint32_t i = 0; i < superpage; i++) {
      pFIL->erase(FIL::Request(invlpn, param->makePPN(psbn, i, 0), 0,
                               eventEraseDone, 0));
    }

    targetBlock.beginAt = now;
    targetBlock.writeCounter = superpage;  // Reuse
    stat.erasedBlocks += superpage;
  }
}

void BasicReadReclaim::translate(uint64_t now) {
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
          "RR    | READ  | PSBN %" PRIx64 "h | PSPN %" PRIx64
          "h -> LSPN %" PRIx64 "h | %" PRIu64 " - %" PRIu64 " (%" PRIu64 ")",
          targetBlock.blockID, param->getPSPNFromPPN(ppn),
          param->getLSPNFromLPN(lpn), ctx.beginAt, now, now - ctx.beginAt);
    }
    else {
      debugprint(
          logid,
          "RR    | READ  | PBN %" PRIx64 "h | PPN %" PRIx64 "h -> LPN %" PRIx64
          "h | %" PRIu64 " - %" PRIu64 " (%" PRIu64 ")",
          targetBlock.blockID, ppn, lpn, ctx.beginAt, now, now - ctx.beginAt);
    }

    ftlobject.pMapping->writeMapping(&ctx.request, eventDoWrite, true);
  }
}

void BasicReadReclaim::write(uint64_t now) {
  auto &ctx = targetBlock.copyList.at(targetBlock.pageWriteIndex++);
  auto lpn = ctx.request.getLPN();
  auto ppn = ctx.request.getPPN();

  if (superpage > 1) {
    debugprint(logid,
               "RR    | WRITE | PSBN %" PRIx64 "h | LSPN %" PRIx64
               "h -> PSPN %" PRIx64 "h",
               targetBlock.blockID, param->getLSPNFromLPN(lpn),
               param->getPSPNFromPPN(ppn));
  }
  else {
    debugprint(logid,
               "RR    | WRITE | PBN %" PRIx64 "h | LPN %" PRIx64
               "h -> PPN %" PRIx64 "h",
               targetBlock.blockID, lpn, ppn);
  }

  for (uint32_t i = 0; i < superpage; i++) {
    pFIL->program(FIL::Request(static_cast<LPN>(lpn + i),
                               static_cast<PPN>(ppn + i), makeBufferAddress(i),
                               eventWriteDone, 0));
  }

  targetBlock.writeCounter += superpage;  // Do not overwrite
  ctx.beginAt = now;
}

void BasicReadReclaim::writeDone(uint64_t now) {
  targetBlock.writeCounter--;

  if (targetBlock.writeCounter == 0) {
    auto &ctx = targetBlock.copyList.at(targetBlock.pageWriteIndex - 1);
    auto lpn = ctx.request.getLPN();
    auto ppn = ctx.request.getPPN();

    if (superpage > 1) {
      debugprint(
          logid,
          "RR    | WRITE | PSBN %" PRIx64 "h | LSPN %" PRIx64
          "h -> PSPN %" PRIx64 "h | %" PRIu64 " - %" PRIu64 " (%" PRIu64 ")",
          targetBlock.blockID, param->getLSPNFromLPN(lpn),
          param->getPSPNFromPPN(ppn), ctx.beginAt, now, now - ctx.beginAt);
    }
    else {
      debugprint(
          logid,
          "RR    | WRITE | PBN %" PRIx64 "h | LPN %" PRIx64 "h -> PPN %" PRIx64
          "h | %" PRIu64 " - %" PRIu64 " (%" PRIu64 ")",
          targetBlock.blockID, lpn, ppn, ctx.beginAt, now, now - ctx.beginAt);
    }

    // Go back to read
    scheduleNow(eventDoRead);
  }
}

void BasicReadReclaim::eraseDone(uint64_t now) {
  targetBlock.writeCounter--;

  if (targetBlock.writeCounter == 0) {
    PSBN psbn = targetBlock.blockID;

    // Erase completed
    if (superpage > 1) {
      debugprint(logid,
                 "RR    | ERASE | PSBN %" PRIx64 "h | %" PRIu64 " - %" PRIu64
                 " (%" PRIu64 ")",
                 psbn, targetBlock.beginAt, now, now - targetBlock.beginAt);
    }
    else {
      debugprint(logid,
                 "RR    | ERASE | PBN %" PRIx64 "h | %" PRIu64 " - %" PRIu64
                 " (%" PRIu64 ")",
                 psbn, targetBlock.beginAt, now, now - targetBlock.beginAt);
    }

    // Mark table/block as erased
    ftlobject.pAllocator->reclaimBlocks(psbn, eventDone, 0);
  }
}

void BasicReadReclaim::done(uint64_t now) {
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

    ftlobject.pAllocator->getVictimBlocks(targetBlock, nullptr, eventDoRead, 0);
  }
  else {
    state = State::Idle;
  }
}

bool BasicReadReclaim::doErrorCheck(const PPN &ppn) {
  uint64_t now = getTick();

  auto pspn = param->getPSPNFromPPN(ppn);
  auto psbn = param->getPSBNFromPSPN(pspn);
  auto nbiterr = estimateBitError(now, psbn);

  // TODO: hard-coded
  if (nbiterr >= 50) {
    if (state < State::Foreground) {
      targetBlock.blockID = psbn;

      ftlobject.pAllocator->getVictimBlocks(targetBlock, nullptr, eventDoRead,
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
  targetBlock.createCheckpoint(out);

  BACKUP_SCALAR(out, beginAt);
  BACKUP_SCALAR(out, stat);

  BACKUP_STL(out, pendingList, iter, BACKUP_SCALAR(out, iter.second));

  BACKUP_EVENT(out, eventDoRead);
  BACKUP_EVENT(out, eventDoTranslate);
  BACKUP_EVENT(out, eventDoWrite);
  BACKUP_EVENT(out, eventWriteDone);
  BACKUP_EVENT(out, eventEraseDone);
  BACKUP_EVENT(out, eventDone);
}

void BasicReadReclaim::restoreCheckpoint(std::istream &in) noexcept {
  targetBlock.restoreCheckpoint(in);

  RESTORE_SCALAR(in, beginAt);
  RESTORE_SCALAR(in, stat);

  RESTORE_STL_RESERVE(in, pendingList, iter, {
    PSBN psbn;

    RESTORE_SCALAR(in, psbn);

    pendingList.push_back(psbn, psbn);
  });

  RESTORE_EVENT(in, eventDoRead);
  RESTORE_EVENT(in, eventDoTranslate);
  RESTORE_EVENT(in, eventDoWrite);
  RESTORE_EVENT(in, eventWriteDone);
  RESTORE_EVENT(in, eventEraseDone);
  RESTORE_EVENT(in, eventDone);
}

}  // namespace SimpleSSD::FTL::ReadReclaim
