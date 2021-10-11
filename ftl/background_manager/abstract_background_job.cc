// SPDX-License-Identifier: LGPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "ftl/background_manager/abstract_background_job.hh"

#include "ftl/allocator/abstract_allocator.hh"
#include "ftl/mapping/abstract_mapping.hh"

namespace SimpleSSD::FTL {

AbstractJob::AbstractJob(ObjectData &o, FTLObjectData &fo)
    : Object(o), ftlobject(fo) {}

AbstractJob::~AbstractJob() {}

void AbstractJob::triggerByUser(TriggerType, Request *) {}

void AbstractJob::triggerByIdle(uint64_t, uint64_t) {}

AbstractBlockCopyJob::AbstractBlockCopyJob(ObjectData &o, FTLObjectData &fo,
                                           FIL::FIL *fil)
    : AbstractJob(o, fo),
      pFIL(fil),
      param(ftlobject.pMapping->getInfo()),
      bufferBaseAddress(0),
      superpage(param->pageSize),
      pageSize(object.config->getNANDStructure()->pageSize),
      logid(getDebugLogID()),
      logprefix(getLogPrefix()) {
  const auto size = getParallelBlockCount();
  auto prefix = getPrefix();

  targetBlocks.resize(size);

  // Memory allocation
  auto &_bufferBaseAddress = const_cast<uint64_t &>(bufferBaseAddress);
  auto required = superpage * pageSize * size;

  if (object.memory->allocate(required, Memory::MemoryType::SRAM, "", true) ==
      0) {
    _bufferBaseAddress = object.memory->allocate(
        required, Memory::MemoryType::SRAM, prefix + "::Buffer");
  }
  else {
    _bufferBaseAddress = object.memory->allocate(
        required, Memory::MemoryType::DRAM, prefix + "::Buffer");
  }

  // Create events
  eventReadPage =
      createEvent([this](uint64_t t, uint64_t d) { readPage(t, d); },
                  prefix + "::eventReadPage");
  eventUpdateMapping =
      createEvent([this](uint64_t t, uint64_t d) { updateMapping(t, d); },
                  prefix + "::eventUpdateMapping");
  eventWritePage =
      createEvent([this](uint64_t t, uint64_t d) { writePage(t, d); },
                  prefix + "::eventWritePage");
  eventWriteDone =
      createEvent([this](uint64_t t, uint64_t d) { writeDone(t, d); },
                  prefix + "::eventWriteDone");
  eventEraseDone =
      createEvent([this](uint64_t t, uint64_t d) { eraseDone(t, d); },
                  prefix + "::eventEraseDone");
  eventDone = createEvent([this](uint64_t t, uint64_t d) { done(t, d); },
                          prefix + "::eventDone");
}

AbstractBlockCopyJob::~AbstractBlockCopyJob() {}

void AbstractBlockCopyJob::readPage(uint64_t now, uint32_t blockIndex) {
  auto &targetBlock = targetBlocks[blockIndex];

  if (LIKELY(targetBlock.pageReadIndex < targetBlock.copyList.size())) {
    auto &ctx = targetBlock.copyList.at(targetBlock.pageReadIndex++);
    auto ppn = param->makePPN(targetBlock.blockID, 0, ctx.pageIndex);

    if (superpage > 1) {
      debugprint(logid, "%s| READ  | PSBN %" PRIx64 "h | PSPN %" PRIx64 "h",
                 logprefix, targetBlock.blockID, param->getPSPNFromPPN(ppn));
    }
    else {
      debugprint(logid, "%s| READ  | PBN %" PRIx64 "h | PPN %" PRIx64 "h",
                 logprefix, targetBlock.blockID, ppn);
    }

    for (uint32_t i = 0; i < superpage; i++) {
      ppn = param->makePPN(targetBlock.blockID, i, ctx.pageIndex);

      // Fill request
      if (i == 0) {
        ctx.request.setTag(blockIndex);
        ctx.request.setPPN(ppn);
        ctx.request.setDRAMAddress(makeBufferAddress(blockIndex, i));

        pFIL->read(FIL::Request(&ctx.request, eventUpdateMapping));
      }
      else {
        pFIL->read(FIL::Request(LPN{}, ppn, makeBufferAddress(blockIndex, i),
                                eventUpdateMapping, blockIndex));
      }
    }

    targetBlock.readCounter = superpage;
    ctx.beginAt = now;
  }
  else {
    // Do erase
    PSBN psbn = targetBlock.blockID;

    // Erase
    if (superpage > 1) {
      debugprint(logid, "%s| ERASE | PSBN %" PRIx64 "h", logprefix, psbn);
    }
    else {
      // PSBN == PBN when superpage == 1
      debugprint(logid, "%s| ERASE | PBN %" PRIx64 "h", logprefix, psbn);
    }

    for (uint32_t i = 0; i < superpage; i++) {
      pFIL->erase(FIL::Request(LPN{}, param->makePPN(psbn, i, 0), 0,
                               eventEraseDone, blockIndex));
    }

    targetBlock.beginAt = now;
    targetBlock.writeCounter = superpage;  // Reuse
  }
}

void AbstractBlockCopyJob::updateMapping(uint64_t now, uint32_t blockIndex) {
  auto &targetBlock = targetBlocks[blockIndex];

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
        BlockAllocator::AllocationStrategy::LowestEraseCount);
  }
}

void AbstractBlockCopyJob::writePage(uint64_t now, uint32_t blockIndex) {
  auto &targetBlock = targetBlocks[blockIndex];

  auto &ctx = targetBlock.copyList.at(targetBlock.pageWriteIndex++);
  auto lpn = ctx.request.getLPN();
  auto ppn = ctx.request.getPPN();

  if (superpage > 1) {
    debugprint(logid,
               "%s| WRITE | PSBN %" PRIx64 "h | LSPN %" PRIx64
               "h -> PSPN %" PRIx64 "h",
               logprefix, targetBlock.blockID, param->getLSPNFromLPN(lpn),
               param->getPSPNFromPPN(ppn));
  }
  else {
    debugprint(logid,
               "%s| WRITE | PBN %" PRIx64 "h | LPN %" PRIx64 "h -> PPN %" PRIx64
               "h",
               logprefix, targetBlock.blockID, lpn, ppn);
  }

  for (uint32_t i = 0; i < superpage; i++) {
    pFIL->program(FIL::Request(
        static_cast<LPN>(lpn + i), static_cast<PPN>(ppn + i),
        makeBufferAddress(blockIndex, i), eventWriteDone, blockIndex));
  }

  targetBlock.writeCounter += superpage;  // Do not overwrite
  ctx.beginAt = now;
}

void AbstractBlockCopyJob::writeDone(uint64_t now, uint32_t blockIndex) {
  auto &targetBlock = targetBlocks[blockIndex];

  targetBlock.writeCounter--;

  if (targetBlock.writeCounter == 0) {
    auto &ctx = targetBlock.copyList.at(targetBlock.pageWriteIndex - 1);
    auto lpn = ctx.request.getLPN();
    auto ppn = ctx.request.getPPN();

    if (superpage > 1) {
      debugprint(
          logid,
          "%s| WRITE | PSBN %" PRIx64 "h | LSPN %" PRIx64 "h -> PSPN %" PRIx64
          "h | %" PRIu64 " - %" PRIu64 " (%" PRIu64 ")",
          logprefix, targetBlock.blockID, param->getLSPNFromLPN(lpn),
          param->getPSPNFromPPN(ppn), ctx.beginAt, now, now - ctx.beginAt);
    }
    else {
      debugprint(logid,
                 "%s| WRITE | PBN %" PRIx64 "h | LPN %" PRIx64
                 "h -> PPN %" PRIx64 "h | %" PRIu64 " - %" PRIu64 " (%" PRIu64
                 ")",
                 logprefix, targetBlock.blockID, lpn, ppn, ctx.beginAt, now,
                 now - ctx.beginAt);
    }

    // Go back to read
    scheduleNow(eventReadPage, blockIndex);
  }
}

void AbstractBlockCopyJob::eraseDone(uint64_t now, uint32_t blockIndex) {
  auto &targetBlock = targetBlocks[blockIndex];

  targetBlock.writeCounter--;

  if (targetBlock.writeCounter == 0) {
    // Erase completed
    if (superpage > 1) {
      debugprint(logid,
                 "%s| ERASE | PSBN %" PRIx64 "h | %" PRIu64 " - %" PRIu64
                 " (%" PRIu64 ")",
                 logprefix, targetBlock.blockID, targetBlock.beginAt, now,
                 now - targetBlock.beginAt);
    }
    else {
      debugprint(logid,
                 "%s| ERASE | PBN %" PRIx64 "h | %" PRIu64 " - %" PRIu64
                 " (%" PRIu64 ")",
                 logprefix, targetBlock.blockID, targetBlock.beginAt, now,
                 now - targetBlock.beginAt);
    }

    // Mark table/block as erased
    ftlobject.pAllocator->reclaimBlock(targetBlock.blockID, eventDone,
                                       blockIndex);
  }
}

void AbstractBlockCopyJob::createCheckpoint(std::ostream &out) const noexcept {
  BACKUP_STL(out, targetBlocks, iter, iter.createCheckpoint(out););

  BACKUP_EVENT(out, eventReadPage);
  BACKUP_EVENT(out, eventUpdateMapping);
  BACKUP_EVENT(out, eventWritePage);
  BACKUP_EVENT(out, eventWriteDone);
  BACKUP_EVENT(out, eventEraseDone);
  BACKUP_EVENT(out, eventDone);
}

void AbstractBlockCopyJob::restoreCheckpoint(std::istream &in) noexcept {
  RESTORE_STL_RESIZE(in, targetBlocks, iter, iter.restoreCheckpoint(in););

  RESTORE_EVENT(in, eventReadPage);
  RESTORE_EVENT(in, eventUpdateMapping);
  RESTORE_EVENT(in, eventWritePage);
  RESTORE_EVENT(in, eventWriteDone);
  RESTORE_EVENT(in, eventEraseDone);
  RESTORE_EVENT(in, eventDone);
}

}  // namespace SimpleSSD::FTL
