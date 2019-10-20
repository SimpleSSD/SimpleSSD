// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "ftl/base/basic_ftl.hh"

namespace SimpleSSD::FTL {

BasicFTL::BasicFTL(ObjectData &o, HIL::CommandManager *m, FIL::FIL *f)
    : AbstractFTL(o, m, f),
      gcInProgress(false),
      gcBlock(o.config->getNANDStructure()->page),
      formatInProgress(0) {
  auto pageSize = object.config->getNANDStructure()->pageSize;
  gcBuffer = (uint8_t *)calloc(pageSize, 1);

  pMapper->initialize(pAllocator);
}

BasicFTL::~BasicFTL() {
  free(gcBuffer);
}

void BasicFTL::read_find(uint64_t tag) {
  pMapper->readMapping(std::move(req), eventReadMappingDone);
}

void BasicFTL::read_dofil(uint64_t tag) {
  auto &ctx = pMapper->getContext(tag);

  // Now we have PPN
  pFIL->submit(FIL::Request(ctx.req.id, eventReadFILDone, tag,
                            FIL::Operation::Read, ctx.address, ctx.req.buffer,
                            ctx.spare));
}

void BasicFTL::read_done(uint64_t tag) {
  auto &ctx = pMapper->getContext(tag);

  scheduleNow(ctx.req.eid, ctx.req.data);

  pMapper->releaseContext(tag);
}

void BasicFTL::write_find(uint64_t tag) {
  pMapper->writeMapping(std::move(req), eventWriteMappingDone);
}

void BasicFTL::write_dofil(uint64_t tag) {
  auto &ctx = pMapper->getContext(tag);

  // Now we have PPN
  pFIL->submit(FIL::Request(ctx.req.id, eventReadFILDone, tag,
                            FIL::Operation::Program, ctx.address,
                            ctx.req.buffer, ctx.spare));
}

void BasicFTL::write_done(uint64_t tag) {
  auto &ctx = pMapper->getContext(tag);

  scheduleNow(ctx.req.eid, ctx.req.data);

  pMapper->releaseContext(tag);

  // Check GC threshold for On-demand GC
  if (pMapper->checkGCThreshold() && formatInProgress == 0) {
    scheduleNow(eventGCBegin);
  }
}

void BasicFTL::invalidate_find(uint64_t tag) {
  pMapper->invalidateMapping(std::move(req), eventInvalidateMappingDone);
}

void BasicFTL::invalidate_dofil(uint64_t tag) {
  auto &ctx = pMapper->getContext(tag);

  if (ctx.req.opcode == Operation::Trim) {
    // Complete here (not erasing blocks)
    scheduleNow(ctx.req.eid, ctx.req.data);
  }
  else if (formatInProgress == 0) {
    // Erase block here
    formatInProgress = 100;  // 100% remain
    fctx.eid = ctx.req.eid;
    fctx.data = ctx.req.data;

    // Make list
    pMapper->getBlocks(ctx.req.address, ctx.req.length, gcList,
                       eventGCListDone);
  }
  else {
    // TODO: Handle this case
    scheduleNow(ctx.req.eid, ctx.req.data);
  }

  pMapper->releaseContext(tag);
}

void BasicFTL::gc_trigger() {
  gcInProgress = true;
  formatInProgress = 100;

  // Get block list from allocator
  pAllocator->getVictimBlocks(gcList, eventGCListDone);
}

void BasicFTL::gc_blockinfo() {
  if (LIKELY(gcList.size() > 0)) {
    PPN block = gcList.front();

    gcList.pop_front();
    gcBlock.blockID = block;
    nextCopyIndex = 0;

    pMapper->getBlockInfo(gcBlock, eventGCRead);
  }
  else {
    scheduleNow(eventGCDone);
  }
}

void BasicFTL::gc_read() {
  // Find valid page
  if (LIKELY(nextCopyIndex < gcBlock.valid.size())) {
    while (!gcBlock.valid.test(nextCopyIndex++))
      ;

    if (LIKELY(nextCopyIndex < gcBlock.valid.size())) {
      auto &mapping = gcBlock.lpnList.at(nextCopyIndex);

      pFIL->submit(FIL::Request(0, eventGCWriteMapping, 0, FIL::Operation::Read,
                                mapping.second, gcBuffer));
    }

    return;
  }

  // Done copy
  scheduleNow(eventGCErase);
}

void BasicFTL::gc_write() {
  auto &mapping = gcBlock.lpnList.at(nextCopyIndex);

  pMapper->writeMapping(mapping, eventGCWrite);
}

void BasicFTL::gc_writeDofil() {
  auto &mapping = gcBlock.lpnList.at(nextCopyIndex++);

  pFIL->submit(FIL::Request(0, eventGCRead, 0, FIL::Operation::Program,
                            mapping.second, gcBuffer));
}

void BasicFTL::gc_erase() {
  pFIL->submit(FIL::Request(0, eventGCListDone, 0, FIL::Operation::Erase,
                            gcBlock.blockID, nullptr));
}

void BasicFTL::gc_done() {
  formatInProgress = 0;

  if (!gcInProgress) {
    // This was format
    scheduleNow(fctx.eid, fctx.data);
  }

  gcInProgress = false;
}

void BasicFTL::submit(uint64_t) {
  switch (req.opcode) {
    case Operation::Read:
      read_find(std::move(req));

      break;
    case Operation::Write:
      write_find(std::move(req));

      break;
    case Operation::Trim:
    case Operation::Format:
      invalidate_find(std::move(req));
      break;
  }
}

bool BasicFTL::isGC() {
  return gcInProgress;
}

uint8_t BasicFTL::isFormat() {
  if (gcInProgress) {
    return 0;
  }
  else {
    return formatInProgress;
  }
}

}  // namespace SimpleSSD::FTL
