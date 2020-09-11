// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 *         Junhyeok Jang <jhjang@camelab.org>
 */

#include "ftl/mapping/page_level_mapping.hh"

#include <random>

#include "ftl/allocator/abstract_allocator.hh"
#include "ftl/base/abstract_ftl.hh"

namespace SimpleSSD::FTL::Mapping {

PageLevelMapping::PageLevelMapping(ObjectData &o)
    : AbstractMapping(o),
      totalPhysicalSuperPages(param.totalPhysicalPages / param.superpage),
      totalPhysicalSuperBlocks(param.totalPhysicalBlocks / param.superpage),
      totalLogicalSuperPages(param.totalLogicalPages / param.superpage),
      table(nullptr),
      blockMetadata(nullptr) {
  // Check spare size
  panic_if(filparam->spareSize < sizeof(LPN), "NAND spare area is too small.");
}

PageLevelMapping::~PageLevelMapping() {
  free(table);
  delete[] blockMetadata;
}

void PageLevelMapping::physicalSuperPageStats(uint64_t &valid,
                                              uint64_t &invalid) {
  valid = 0;
  invalid = 0;

  for (uint64_t i = 0; i < totalPhysicalSuperBlocks; i++) {
    auto &block = blockMetadata[i];

    if (block.nextPageToWrite > 0) {
      valid += block.validPages.count();

      for (uint32_t i = 0; i < block.nextPageToWrite; i++) {
        if (!block.validPages.test(i)) {
          ++invalid;
        }
      }
    }
  }
}

CPU::Function PageLevelMapping::readMappingInternal(LSPN lspn, PSPN &pspn) {
  CPU::Function fstat;
  CPU::markFunction(fstat);

  panic_if(lspn >= totalLogicalSuperPages, "LPN out of range.");

  // Read entry
  auto entry = readTableEntry(table, lspn);
  auto valid = parseTableEntry(entry);

  insertMemoryAddress(true, makeTableAddress(lspn), entrySize);

  if (valid) {
    pspn = static_cast<PSPN>(entry);

    // Update accessed time
    PSBN block = param.getPSBNFromPSPN(pspn);
    blockMetadata[block].insertedAt = getTick();

    insertMemoryAddress(false, makeMetadataAddress(block), 2);
  }
  else {
    pspn.invalidate();
  }

  return fstat;
}

CPU::Function PageLevelMapping::writeMappingInternal(LSPN lspn, PSPN &pspn,
                                                     bool init) {
  CPU::Function fstat;
  CPU::markFunction(fstat);

  panic_if(lspn >= totalLogicalSuperPages, "LPN out of range.");

  auto entry = readTableEntry(table, lspn);
  auto valid = parseTableEntry(entry);

  insertMemoryAddress(true, makeTableAddress(lspn), entrySize, !init);

  if (valid) {
    // This is valid entry, invalidate block
    PSPN old = static_cast<PSPN>(entry);

    PSBN block = param.getPSBNFromPSPN(old);
    PSBN page = param.getPSBNFromPSPN(old);

    blockMetadata[block].validPages.reset(page);

    // Memory timing after demand paging
    insertMemoryAddress(false, makeMetadataAddress(block) + 4 + page / 8, 1,
                        !init);
  }

  // Get block from allocated block pool
  PSBN blockID = allocator->getBlockAt(std::numeric_limits<uint32_t>::max());
  auto block = &blockMetadata[blockID];

  // Check we have to get new block
  if (block->nextPageToWrite == filparam->page) {
    // Get a new block
    fstat += allocator->allocateBlock(blockID);

    block = &blockMetadata[blockID];
  }

  // Get new page
  block->validPages.set(block->nextPageToWrite);
  insertMemoryAddress(
      false,
      makeMetadataAddress(block->blockID) + 4 + block->nextPageToWrite / 8, 1,
      !init);

  pspn = param.makePSPN(block->blockID, block->nextPageToWrite++);

  block->insertedAt = getTick();
  insertMemoryAddress(false, makeMetadataAddress(block->blockID), 4, !init);

  // Write entry
  entry = makeTableEntry(pspn, 1);
  writeTableEntry(table, lspn, entry);

  insertMemoryAddress(false, makeTableAddress(lspn), entrySize, !init);

  return fstat;
}

CPU::Function PageLevelMapping::invalidateMappingInternal(LSPN lspn,
                                                          PSPN &pspn) {
  CPU::Function fstat;
  CPU::markFunction(fstat);

  panic_if(lspn >= totalLogicalSuperPages, "LPN out of range.");

  auto entry = readTableEntry(table, lspn);
  auto valid = parseTableEntry(entry);

  if (valid) {
    // Hack: Prevent multiple memory accesses when using superpage
    insertMemoryAddress(true, makeTableAddress(lspn), entrySize);

    // Invalidate block
    PSBN block = param.getPSBNFromPSPN(pspn);
    PSBN page = param.getPSBNFromPSPN(pspn);

    blockMetadata[block].validPages.reset(page);
    insertMemoryAddress(false, makeMetadataAddress(block) + 4 + page / 8, 1);

    insertMemoryAddress(false, makeTableAddress(lspn), entrySize);

    // Invalidate entry
    pspn.invalidate();

    writeTableEntry(table, lspn, 0);
  }

  return fstat;
}

void PageLevelMapping::initialize(AbstractFTL *f,
                                  BlockAllocator::AbstractAllocator *a) {
  AbstractMapping::initialize(f, a);

  // Initialize memory
  {
    // Allocate table and block metadata
    entrySize = makeEntrySize(totalLogicalSuperPages, 1, readTableEntry,
                              writeTableEntry, parseTableEntry, makeTableEntry);

    table = (uint8_t *)calloc(totalLogicalSuperPages, entrySize);
    blockMetadata = new BlockMetadata<PSBN>[totalPhysicalSuperBlocks]();

    panic_if(!table, "Memory mapping for mapping table failed.");
    panic_if(!blockMetadata, "Memory mapping for block metadata failed.");

    // Valid page bits (packed) + 2byte clock + 2byte page offset
    metadataEntrySize = DIVCEIL(filparam->page, 8) + 4;

    metadataBaseAddress = object.memory->allocate(
        totalPhysicalSuperBlocks * metadataEntrySize, Memory::MemoryType::DRAM,
        "FTL::Mapping::PageLevelMapping::BlockMeta");

    tableBaseAddress = object.memory->allocate(
        totalLogicalSuperPages * entrySize, Memory::MemoryType::DRAM,
        "FTL::Mapping::PageLevelMapping::Table", true);

    // Retry allocation
    tableBaseAddress = object.memory->allocate(
        totalLogicalSuperPages * entrySize, Memory::MemoryType::DRAM,
        "FTL::Mapping::PageLevelMapping::Table");

    // Fill metadata
    for (uint64_t i = 0; i < totalPhysicalSuperBlocks; i++) {
      blockMetadata[i] =
          BlockMetadata<PSBN>(static_cast<PSBN>(i), filparam->page);
    }

    // Memory usage information
    debugprint(Log::DebugID::FTL_PageLevel, "Memory usage:");
    debugprint(Log::DebugID::FTL_PageLevel, " Mapping table: %" PRIu64,
               totalLogicalSuperPages * entrySize);
    debugprint(Log::DebugID::FTL_PageLevel, " Block metatdata: %" PRIu64,
               totalPhysicalSuperBlocks * metadataEntrySize);
  }

  // Make free block pool in allocator
  for (uint64_t i = 0; i < param.parallelism; i += param.superpage) {
    PSBN tmp;

    allocator->allocateBlock(tmp);
  }

  // Perform filling
  uint64_t nPagesToWarmup;
  uint64_t nPagesToInvalidate;
  uint64_t maxPagesBeforeGC;
  uint64_t valid;
  uint64_t invalid;
  Config::FillingType mode;
  PSPN pspn;
  std::vector<uint8_t> spare;

  debugprint(Log::DebugID::FTL_PageLevel, "Initialization started");

  nPagesToWarmup = (uint64_t)(
      totalLogicalSuperPages *
      readConfigFloat(Section::FlashTranslation, Config::Key::FillRatio));
  nPagesToInvalidate = (uint64_t)(
      totalLogicalSuperPages * readConfigFloat(Section::FlashTranslation,
                                               Config::Key::InvalidFillRatio));
  mode = (Config::FillingType)readConfigUint(Section::FlashTranslation,
                                             Config::Key::FillingMode);
  maxPagesBeforeGC = (uint64_t)(
      filparam->page * (totalPhysicalSuperBlocks *
                        (1.f - readConfigFloat(Section::FlashTranslation,
                                               Config::Key::GCThreshold))));

  if (nPagesToWarmup + nPagesToInvalidate > maxPagesBeforeGC) {
    warn("ftl: Too high filling ratio. Adjusting invalidPageRatio.");
    nPagesToInvalidate = maxPagesBeforeGC - nPagesToWarmup;
  }

  debugprint(Log::DebugID::FTL_PageLevel, "Total logical pages: %" PRIu64,
             totalLogicalSuperPages);
  debugprint(Log::DebugID::FTL_PageLevel,
             "Total logical pages to fill: %" PRIu64 " (%.2f %%)",
             nPagesToWarmup, nPagesToWarmup * 100.f / totalLogicalSuperPages);
  debugprint(Log::DebugID::FTL_PageLevel,
             "Total invalidated pages to create: %" PRIu64 " (%.2f %%)",
             nPagesToInvalidate,
             nPagesToInvalidate * 100.f / totalLogicalSuperPages);

  // Step 1. Filling
  if (mode == Config::FillingType::SequentialSequential ||
      mode == Config::FillingType::SequentialRandom) {
    // Sequential
    for (uint64_t i = 0; i < nPagesToWarmup; i++) {
      writeMappingInternal(static_cast<LSPN>(i), pspn, true);

      for (uint32_t j = 0; j < param.superpage; j++) {
        auto _ppn = param.makePPN(pspn, j);
        auto _lpn = i * param.superpage + j;

        pFTL->writeSpare(_ppn, (uint8_t *)&_lpn, sizeof(LPN));
      }
    }
  }
  else {
    // Random
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dist(0, totalLogicalSuperPages - 1);

    for (uint64_t i = 0; i < nPagesToWarmup; i++) {
      LSPN lspn = static_cast<LSPN>(dist(gen));

      writeMappingInternal(lspn, pspn, true);

      for (uint32_t j = 0; j < param.superpage; j++) {
        auto _ppn = param.makePPN(pspn, j);
        auto _lpn = i * param.superpage + j;

        pFTL->writeSpare(_ppn, (uint8_t *)&_lpn, sizeof(LPN));
      }
    }
  }

  // Step 2. Invalidating
  if (mode == Config::FillingType::SequentialSequential) {
    // Sequential
    for (uint64_t i = 0; i < nPagesToInvalidate; i++) {
      writeMappingInternal(static_cast<LSPN>(i), pspn, true);

      for (uint32_t j = 0; j < param.superpage; j++) {
        auto _ppn = param.makePPN(pspn, j);
        auto _lpn = i * param.superpage + j;

        pFTL->writeSpare(_ppn, (uint8_t *)&_lpn, sizeof(LPN));
      }
    }
  }
  else if (mode == Config::FillingType::SequentialRandom) {
    // Random
    // We can successfully restrict range of LPN to create exact number of
    // invalid pages because we wrote in sequential mannor in step 1.
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dist(0, nPagesToWarmup - 1);

    for (uint64_t i = 0; i < nPagesToInvalidate; i++) {
      LSPN lspn = static_cast<LSPN>(dist(gen));

      writeMappingInternal(lspn, pspn, true);

      for (uint32_t j = 0; j < param.superpage; j++) {
        auto _ppn = param.makePPN(pspn, j);
        auto _lpn = i * param.superpage + j;

        pFTL->writeSpare(_ppn, (uint8_t *)&_lpn, sizeof(LPN));
      }
    }
  }
  else {
    // Random
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dist(0, totalLogicalSuperPages - 1);

    for (uint64_t i = 0; i < nPagesToInvalidate; i++) {
      LSPN lspn = static_cast<LSPN>(dist(gen));

      writeMappingInternal(lspn, pspn, true);

      for (uint32_t j = 0; j < param.superpage; j++) {
        auto _ppn = param.makePPN(pspn, j);
        auto _lpn = i * param.superpage + j;

        pFTL->writeSpare(_ppn, (uint8_t *)&_lpn, sizeof(LPN));
      }
    }
  }

  // Report
  physicalSuperPageStats(valid, invalid);
  debugprint(Log::DebugID::FTL_PageLevel, "Filling finished. Page status:");
  debugprint(Log::DebugID::FTL_PageLevel,
             "  Total valid physical pages: %" PRIu64
             " (%.2f %%, target: %" PRIu64 ", error: %" PRId64 ")",
             valid, valid * 100.f / totalLogicalSuperPages, nPagesToWarmup,
             (int64_t)(valid - nPagesToWarmup));
  debugprint(Log::DebugID::FTL_PageLevel,
             "  Total invalid physical pages: %" PRIu64
             " (%.2f %%, target: %" PRIu64 ", error: %" PRId64 ")",
             invalid, invalid * 100.f / totalLogicalSuperPages,
             nPagesToInvalidate, (int64_t)(invalid - nPagesToInvalidate));
  debugprint(Log::DebugID::FTL_PageLevel, "Initialization finished");
}

uint64_t PageLevelMapping::getPageUsage(LPN slpn, uint64_t nlp) {
  uint64_t count = 0;

  // Convert to SLPN
  slpn /= param.superpage;
  nlp = DIVCEIL(nlp, param.superpage);

  panic_if(slpn + nlp > totalLogicalSuperPages, "LPN out of range.");

  for (uint64_t i = slpn; i < nlp; i++) {
    auto entry = readTableEntry(table, i);
    auto valid = parseTableEntry(entry);

    if (valid) {
      count++;
    }
  }

  // Convert to LPN
  return count * param.superpage;
}

uint32_t PageLevelMapping::getValidPages(PSBN psbn) {
  return (uint32_t)blockMetadata[psbn].validPages.count();
}

uint64_t PageLevelMapping::getAge(PSBN psbn) {
  return blockMetadata[psbn].insertedAt;
}

void PageLevelMapping::readMapping(Request *cmd, Event eid) {
  CPU::Function fstat;
  CPU::markFunction(fstat);

  // Perform read translation
  LPN lpn = cmd->getLPN();
  LSPN lspn = param.getLSPNFromLPN(lpn);
  uint32_t superpageIndex = param.getSuperpageIndexFromLPN(lpn);
  PPN ppn;
  PSPN pspn;

  requestedReadCount++;
  readLPNCount += param.superpage;

  fstat += readMappingInternal(lspn, pspn);

  if (UNLIKELY(!pspn.isValid())) {
    cmd->setResponse(Response::Unwritten);
  }
  else {
    ppn = param.makePPN(pspn, superpageIndex);
  }

  cmd->setPPN(ppn);

  debugprint(Log::DebugID::FTL_PageLevel,
             "Read  | LPN %" PRIx64 "h -> PPN %" PRIx64 "h", lpn,
             cmd->getPPN());

  requestMemoryAccess(eid, cmd->getTag(), fstat);
}

void PageLevelMapping::writeMapping(Request *cmd, Event eid) {
  CPU::Function fstat;
  CPU::markFunction(fstat);

  // Perform write translation
  LPN lpn = cmd->getLPN();
  LSPN lspn = param.getLSPNFromLPN(lpn);
  uint32_t superpageIndex = param.getSuperpageIndexFromLPN(lpn);
  PSPN pspn;

  requestedWriteCount++;
  writeLPNCount += param.superpage;

  fstat += writeMappingInternal(lspn, pspn);

  cmd->setPPN(param.makePPN(pspn, superpageIndex));

  debugprint(Log::DebugID::FTL_PageLevel,
             "Write | LPN %" PRIx64 "h -> PPN %" PRIx64 "h", lpn,
             cmd->getPPN());

  requestMemoryAccess(eid, cmd->getTag(), fstat);
}

void PageLevelMapping::invalidateMapping(Request *cmd, Event eid) {
  // TODO: consider fullsize of request -- if fullsize is smaller than superpage
  // we should not erase mapping
  panic("Trim/Format not implemented");

  scheduleNow(eid, cmd->getTag());
}

void PageLevelMapping::getMappingSize(uint64_t *min, uint64_t *pre) {
  if (min) {
    *min = param.superpage;
  }
  if (pre) {
    *pre = param.superpage;
  }
}

void PageLevelMapping::getCopyContext(CopyContext &copyctx, Event eid) {
  CPU::Function fstat;
  CPU::markFunction(fstat);

  const auto block = &blockMetadata[copyctx.sblockID];  // superblock

  copyctx.reset();
  copyctx.list.reserve(block->validPages.count());

  for (uint i = 0; i < filparam->page; i++) {
    if (block->validPages.test(i)) {
      // append valid page copy request
      SuperRequest sReq;
      sReq.reserve(param.superpage);
      Request *req;

      for (uint j = 0; j < param.superpage; j++) {
        req = new Request(param.makePPN(copyctx.sblockID, i, j));
        sReq.emplace_back(req);
      }
      copyctx.list.emplace_back(std::move(sReq));
    }
  }

  copyctx.writeCounter.resize(copyctx.list.size(), 0);
  copyctx.copyCounter = copyctx.list.size();
  copyctx.initIter();

  scheduleFunction(CPU::CPUGroup::FlashTranslationLayer, eid, fstat);
}

void PageLevelMapping::markBlockErased(PSBN blockId) {
  blockMetadata[blockId].nextPageToWrite = 0;
  blockMetadata[blockId].validPages.reset();
}

void PageLevelMapping::getStatList(std::vector<Stat> &list,
                                   std::string prefix) noexcept {
  AbstractMapping::getStatList(list, prefix);
}

void PageLevelMapping::getStatValues(std::vector<double> &values) noexcept {
  AbstractMapping::getStatValues(values);
}

void PageLevelMapping::resetStatValues() noexcept {
  AbstractMapping::resetStatValues();
}

void PageLevelMapping::createCheckpoint(std::ostream &out) const noexcept {
  AbstractMapping::createCheckpoint(out);

  BACKUP_SCALAR(out, totalPhysicalSuperPages);
  BACKUP_SCALAR(out, totalPhysicalSuperBlocks);
  BACKUP_SCALAR(out, totalLogicalSuperPages);
  BACKUP_SCALAR(out, entrySize);
  BACKUP_BLOB64(out, table, totalLogicalSuperPages * entrySize);

  for (uint64_t i = 0; i < totalPhysicalSuperBlocks; i++) {
    BACKUP_SCALAR(out, blockMetadata[i].nextPageToWrite);
    BACKUP_SCALAR(out, blockMetadata[i].insertedAt);

    blockMetadata[i].validPages.createCheckpoint(out);
  }
}

void PageLevelMapping::restoreCheckpoint(std::istream &in) noexcept {
  uint64_t tmp64;

  AbstractMapping::restoreCheckpoint(in);

  RESTORE_SCALAR(in, tmp64);
  panic_if(tmp64 != totalPhysicalSuperPages,
           "Invalid FTL configuration while restore.");

  RESTORE_SCALAR(in, tmp64);
  panic_if(tmp64 != totalPhysicalSuperBlocks,
           "Invalid FTL configuration while restore.");

  RESTORE_SCALAR(in, tmp64);
  panic_if(tmp64 != totalLogicalSuperPages,
           "Invalid FTL configuration while restore.");

  RESTORE_SCALAR(in, tmp64);
  panic_if(tmp64 != entrySize, "Invalid FTL configuration while restore.");

  RESTORE_BLOB64(in, table, totalLogicalSuperPages * entrySize);

  for (uint64_t i = 0; i < totalPhysicalSuperBlocks; i++) {
    RESTORE_SCALAR(in, blockMetadata[i].nextPageToWrite);
    RESTORE_SCALAR(in, blockMetadata[i].insertedAt);

    blockMetadata[i].validPages.restoreCheckpoint(in);
  }
}

}  // namespace SimpleSSD::FTL::Mapping
