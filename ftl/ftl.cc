// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "ftl/ftl.hh"

namespace SimpleSSD {

namespace FTL {

FTL::FTL(ObjectData &o) : Object(o), gcInProgress(false), formatInProgress(0) {
  pFIL = new FIL::FIL(object);

  auto pageSize = object.config->getNANDStructure()->pageSize;
  gcBuffer = (uint8_t *)calloc(pageSize, 1);

  switch ((Config::MappingType)readConfigUint(Section::FlashTranslation,
                                              Config::Key::MappingMode)) {
    case Config::MappingType::PageLevelFTL:
      // pMapper = new Mapping::PageLevel(object);

      break;
    case Config::MappingType::BlockLevelFTL:
      // pMapper = new Mapping::BlockLevel(object);

      break;
    case Config::MappingType::VLFTL:
      // pMapper = new Mapping::VLFTL(object);

      break;
  }

  // Currently, we only have default block allocator
  // pAllocator = new BlockAllocator::Default(object, pMapper);

  // Initialize pFTL
  pMapper->initialize(pAllocator);
}

FTL::~FTL() {
  free(gcBuffer);

  delete pFIL;
  delete pMapper;
  delete pAllocator;
}

void FTL::read_find(Request &&req) {
  pMapper->readMapping(std::move(req), eventReadMappingDone);
}

void FTL::read_dofil(uint64_t tag) {
  auto &ctx = pMapper->getContext(tag);

  // Now we have PPN
  pFIL->submit(FIL::Request(ctx.req.id, eventReadFILDone, tag,
                            FIL::Operation::Read, ctx.address, ctx.req.buffer,
                            ctx.spare));
}

void FTL::read_done(uint64_t tag) {
  auto &ctx = pMapper->getContext(tag);

  scheduleNow(ctx.req.eid, ctx.req.data);

  pMapper->releaseContext(tag);
}

void FTL::write_find(Request &&req) {
  pMapper->writeMapping(std::move(req), eventWriteMappingDone);
}

void FTL::write_dofil(uint64_t tag) {
  auto &ctx = pMapper->getContext(tag);

  // Now we have PPN
  pFIL->submit(FIL::Request(ctx.req.id, eventReadFILDone, tag,
                            FIL::Operation::Program, ctx.address,
                            ctx.req.buffer, ctx.spare));
}

void FTL::write_done(uint64_t tag) {
  auto &ctx = pMapper->getContext(tag);

  scheduleNow(ctx.req.eid, ctx.req.data);

  pMapper->releaseContext(tag);

  // Check GC threshold for On-demand GC
  if (pMapper->checkGCThreshold() && !gcInProgress) {
    scheduleNow(eventGCBegin);
  }
}

void FTL::invalidate_find(Request &&req) {
  pMapper->invalidateMapping(std::move(req), eventInvalidateMappingDone);
}

void FTL::invalidate_dofil(uint64_t tag) {
  auto &ctx = pMapper->getContext(tag);

  if (ctx.req.opcode == Operation::Trim) {
    // Complete here (not erasing blocks)
    scheduleNow(ctx.req.eid, ctx.req.data);
  }
  else {
    // Erase block here
    formatInProgress = 100;  // 100% remain
    fctx.begin = ctx.req.address;
    fctx.end = ctx.req.address + ctx.req.length;
    fctx.eid = ctx.req.eid;
    fctx.data = ctx.req.data;

    // TODO: Fill here
  }

  pMapper->releaseContext(tag);
}

void FTL::gc_trigger() {
  gcInProgress = true;

  // Get block list from allocator
  pAllocator->getVictimBlocks(gcList, eventGCListDone);
}

void FTL::gc_blockinfo() {
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

void FTL::gc_read() {
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

void FTL::gc_write() {
  auto &mapping = gcBlock.lpnList.at(nextCopyIndex);

  pMapper->writeMapping(mapping, eventGCWrite);
}

void FTL::gc_writeDofil() {
  auto &mapping = gcBlock.lpnList.at(nextCopyIndex++);

  pFIL->submit(FIL::Request(0, eventGCRead, 0, FIL::Operation::Program,
                            mapping.second, gcBuffer));
}

void FTL::gc_erase() {
  pFIL->submit(FIL::Request(0, eventGCListDone, 0, FIL::Operation::Erase,
                            gcBlock.blockID, nullptr));
}

void FTL::gc_done() {
  gcInProgress = false;
}

void FTL::submit(Request &&req) {
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

Parameter *FTL::getInfo() {
  return pMapper->getInfo();
}

LPN FTL::getPageUsage(LPN lpnBegin, LPN lpnEnd) {
  return pMapper->getPageUsage(lpnBegin, lpnEnd);
}

bool FTL::isGC() {
  return gcInProgress;
}

uint8_t FTL::isFormat() {
  return formatInProgress;
}

void FTL::bypass(FIL::Request &&req) {
  pFIL->submit(std::move(req));
}

void FTL::getStatList(std::vector<Stat> &list, std::string prefix) noexcept {
  pMapper->getStatList(list, prefix + "ftl.mapper.");
  pAllocator->getStatList(list, prefix + "ftl.allocator.");
  pFIL->getStatList(list, prefix);
}

void FTL::getStatValues(std::vector<double> &values) noexcept {
  pMapper->getStatValues(values);
  pAllocator->getStatValues(values);
  pFIL->getStatValues(values);
}

void FTL::resetStatValues() noexcept {
  pMapper->resetStatValues();
  pAllocator->resetStatValues();
  pFIL->resetStatValues();
}

void FTL::createCheckpoint(std::ostream &out) const noexcept {
  pMapper->createCheckpoint(out);
  pAllocator->createCheckpoint(out);
  pFIL->createCheckpoint(out);
}

void FTL::restoreCheckpoint(std::istream &in) noexcept {
  pMapper->restoreCheckpoint(in);
  pAllocator->restoreCheckpoint(in);
  pFIL->restoreCheckpoint(in);
}

}  // namespace FTL

}  // namespace SimpleSSD
