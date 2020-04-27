// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "ftl/mapping/page_level.hh"

#include <random>

#include "ftl/allocator/abstract_allocator.hh"
#include "ftl/base/abstract_ftl.hh"

namespace SimpleSSD::FTL::Mapping {

PageLevel::PageLevel(ObjectData &o)
    : AbstractMapping(o),
      totalPhysicalSuperPages(param.totalPhysicalPages / param.superpage),
      totalPhysicalSuperBlocks(param.totalPhysicalBlocks / param.superpage),
      totalLogicalSuperPages(param.totalLogicalPages / param.superpage),
      table(nullptr),
      blockMetadata(nullptr) {
  // Check spare size
  panic_if(filparam->spareSize < sizeof(LPN), "NAND spare area is too small.");
}

PageLevel::~PageLevel() {
  free(table);
  delete[] blockMetadata;
}

void PageLevel::physicalSuperPageStats(uint64_t &valid, uint64_t &invalid) {
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

CPU::Function PageLevel::readMappingInternal(LPN lspn, PPN &pspn) {
  CPU::Function fstat;
  CPU::markFunction(fstat);

  panic_if(lspn >= totalLogicalSuperPages, "LPN out of range.");

  // Read entry
  auto entry = readTableEntry(table, lspn);
  auto valid = parseTableEntry(entry);

  insertMemoryAddress(true, makeTableAddress(lspn), entrySize);

  if (valid) {
    pspn = entry;

    // Update accessed time
    PPN block = getSuperblockFromSPPN(pspn);
    blockMetadata[block].insertedAt = getTick();

    insertMemoryAddress(false, makeMetadataAddress(block), 2);
  }
  else {
    pspn = InvalidPPN;
  }

  return fstat;
}

CPU::Function PageLevel::writeMappingInternal(LPN lspn, PPN &pspn, bool init) {
  CPU::Function fstat;
  CPU::markFunction(fstat);

  panic_if(lspn >= totalLogicalSuperPages, "LPN out of range.");

  auto entry = readTableEntry(table, lspn);
  auto valid = parseTableEntry(entry);

  insertMemoryAddress(true, makeTableAddress(lspn), entrySize, !init);

  if (valid) {
    // This is valid entry, invalidate block
    PPN old = entry;

    PPN block = getSuperblockFromSPPN(old);
    PPN page = getSuperpageFromSPPN(old);

    blockMetadata[block].validPages.reset(page);

    // Memory timing after demand paging
    insertMemoryAddress(false, makeMetadataAddress(block) + 4 + page / 8, 1,
                        !init);
  }

  // Get block from allocated block pool
  PPN idx = allocator->getBlockAt(InvalidPPN);

  auto block = &blockMetadata[idx];

  // Check we have to get new block
  if (block->nextPageToWrite == filparam->page) {
    // Get a new block
    fstat += allocator->allocateBlock(idx);

    block = &blockMetadata[idx];
  }

  // Get new page
  block->validPages.set(block->nextPageToWrite);
  insertMemoryAddress(
      false,
      makeMetadataAddress(block->blockID) + 4 + block->nextPageToWrite / 8, 1,
      !init);

  pspn = makeSPPN(block->blockID, block->nextPageToWrite++);

  block->insertedAt = getTick();
  insertMemoryAddress(false, makeMetadataAddress(block->blockID), 4, !init);

  // Write entry
  entry = makeTableEntry(pspn, 1);
  writeTableEntry(table, lspn, entry);

  insertMemoryAddress(false, makeTableAddress(lspn), entrySize, !init);

  return fstat;
}

CPU::Function PageLevel::invalidateMappingInternal(LPN lspn, PPN &pspn) {
  CPU::Function fstat;
  CPU::markFunction(fstat);

  panic_if(lspn >= totalLogicalSuperPages, "LPN out of range.");

  auto entry = readTableEntry(table, lspn);
  auto valid = parseTableEntry(entry);

  if (valid) {
    // Hack: Prevent multiple memory accesses when using superpage
    insertMemoryAddress(true, makeTableAddress(lspn), entrySize);

    // Invalidate entry
    pspn = entry;

    writeTableEntry(table, lspn, 0);

    // Invalidate block
    PPN block = getSuperblockFromSPPN(pspn);
    PPN page = getSuperpageFromSPPN(pspn);

    blockMetadata[block].validPages.reset(page);
    insertMemoryAddress(false, makeMetadataAddress(block) + 4 + page / 8, 1);

    insertMemoryAddress(false, makeTableAddress(lspn), entrySize);
  }

  return fstat;
}

void PageLevel::initialize(AbstractFTL *f,
                           BlockAllocator::AbstractAllocator *a) {
  AbstractMapping::initialize(f, a);

  // Initialize memory
  {
    // Allocate table and block metadata
    entrySize = makeEntrySize(totalLogicalSuperPages, 1, readTableEntry,
                              writeTableEntry, parseTableEntry, makeTableEntry);

    table = (uint8_t *)calloc(totalLogicalSuperPages, entrySize);
    blockMetadata = new BlockMetadata[totalPhysicalSuperBlocks]();

    panic_if(!table, "Memory mapping for mapping table failed.");
    panic_if(!blockMetadata, "Memory mapping for block metadata failed.");

    // Valid page bits (packed) + 2byte clock + 2byte page offset
    metadataEntrySize = DIVCEIL(filparam->page, 8) + 4;

    metadataBaseAddress = object.memory->allocate(
        totalPhysicalSuperBlocks * metadataEntrySize, Memory::MemoryType::DRAM,
        "FTL::Mapping::PageLevel::BlockMeta");

    tableBaseAddress = object.memory->allocate(
        totalLogicalSuperPages * entrySize, Memory::MemoryType::DRAM,
        "FTL::Mapping::PageLevel::Table", true);

    // Retry allocation
    tableBaseAddress = object.memory->allocate(
        totalLogicalSuperPages * entrySize, Memory::MemoryType::DRAM,
        "FTL::Mapping::PageLevel::Table");

    // Fill metadata
    for (uint64_t i = 0; i < totalPhysicalSuperBlocks; i++) {
      blockMetadata[i] = BlockMetadata(i, filparam->page);
    }

    // Memory usage information
    debugprint(Log::DebugID::FTL_PageLevel, "Memory usage:");
    debugprint(Log::DebugID::FTL_PageLevel, " Mapping table: %" PRIu64,
               totalLogicalSuperPages * entrySize);
    debugprint(Log::DebugID::FTL_PageLevel, " Block metatdata: %" PRIu64,
               totalPhysicalSuperBlocks * metadataEntrySize);
  }

  // Make free block pool in allocator
  uint64_t parallelism = param.parallelism / param.superpage;

  for (uint64_t i = 0; i < parallelism; i++) {
    PPN tmp = InvalidPPN;

    allocator->allocateBlock(tmp);
  }

  // Perform filling
  uint64_t nPagesToWarmup;
  uint64_t nPagesToInvalidate;
  uint64_t maxPagesBeforeGC;
  uint64_t valid;
  uint64_t invalid;
  Config::FillingType mode;
  LPN _lpn;
  PPN ppn;
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
      writeMappingInternal(i, ppn, true);

      for (uint32_t j = 0; j < param.superpage; j++) {
        _lpn = i * param.superpage + j;

        pFTL->writeSpare(ppn * param.superpage + j, (uint8_t *)&_lpn,
                         sizeof(LPN));
      }
    }
  }
  else {
    // Random
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dist(0, totalLogicalSuperPages - 1);

    for (uint64_t i = 0; i < nPagesToWarmup; i++) {
      LPN lpn = dist(gen);

      writeMappingInternal(lpn, ppn, true);

      for (uint32_t j = 0; j < param.superpage; j++) {
        _lpn = lpn * param.superpage + j;

        pFTL->writeSpare(ppn * param.superpage + j, (uint8_t *)&_lpn,
                         sizeof(LPN));
      }
    }
  }

  // Step 2. Invalidating
  if (mode == Config::FillingType::SequentialSequential) {
    // Sequential
    for (uint64_t i = 0; i < nPagesToInvalidate; i++) {
      writeMappingInternal(i, ppn, true);

      for (uint32_t j = 0; j < param.superpage; j++) {
        _lpn = i * param.superpage + j;

        pFTL->writeSpare(ppn * param.superpage + j, (uint8_t *)&_lpn,
                         sizeof(LPN));
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
      LPN lpn = dist(gen);

      writeMappingInternal(lpn, ppn, true);

      for (uint32_t j = 0; j < param.superpage; j++) {
        _lpn = lpn * param.superpage + j;

        pFTL->writeSpare(ppn * param.superpage + j, (uint8_t *)&_lpn,
                         sizeof(LPN));
      }
    }
  }
  else {
    // Random
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dist(0, totalLogicalSuperPages - 1);

    for (uint64_t i = 0; i < nPagesToInvalidate; i++) {
      LPN lpn = dist(gen);

      writeMappingInternal(lpn, ppn, true);

      for (uint32_t j = 0; j < param.superpage; j++) {
        _lpn = lpn * param.superpage + j;

        pFTL->writeSpare(ppn * param.superpage + j, (uint8_t *)&_lpn,
                         sizeof(LPN));
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

LPN PageLevel::getPageUsage(LPN slpn, LPN nlp) {
  LPN count = 0;

  // Convert to SLPN
  slpn /= param.superpage;
  nlp = DIVCEIL(nlp, param.superpage);

  panic_if(slpn + nlp > totalLogicalSuperPages, "LPN out of range.");

  for (LPN i = slpn; i < nlp; i++) {
    auto entry = readTableEntry(table, i);
    auto valid = parseTableEntry(entry);

    if (valid) {
      count++;
    }
  }

  // Convert to LPN
  return count * param.superpage;
}

uint32_t PageLevel::getValidPages(PPN ppn, uint64_t nlp) {
  panic_if(nlp != param.superpage, "Invalid access from block allocator.");

  ppn /= nlp;

  return (uint32_t)blockMetadata[getSuperblockFromSPPN(ppn)].validPages.count();
}

uint64_t PageLevel::getAge(PPN ppn, uint64_t nlp) {
  panic_if(nlp != param.superpage, "Invalid access from block allocator.");

  ppn /= nlp;

  return blockMetadata[getSuperblockFromSPPN(ppn)].insertedAt;
}

void PageLevel::readMapping(Request *cmd, Event eid) {
  CPU::Function fstat;
  CPU::markFunction(fstat);

  // Perform read translation
  LPN lpn = cmd->getLPN();
  LPN lspn = lpn / param.superpage;
  LPN superpageIndex = lpn % param.superpage;
  PPN pspn = InvalidPPN;

  requestedReadCount++;
  readLPNCount += param.superpage;

  fstat += readMappingInternal(lspn, pspn);

  if (UNLIKELY(pspn == InvalidPPN)) {
    cmd->setResponse(Response::Unwritten);
    cmd->setPPN(InvalidPPN);
  }
  else {
    cmd->setPPN(pspn * param.superpage + superpageIndex);
  }

  debugprint(Log::DebugID::FTL_PageLevel,
             "Read  | LPN %" PRIx64 "h -> PPN %" PRIx64 "h", lpn,
             cmd->getPPN());

  requestMemoryAccess(eid, cmd->getTag(), fstat);
}

void PageLevel::writeMapping(Request *cmd, Event eid) {
  CPU::Function fstat;
  CPU::markFunction(fstat);

  // Check command
  /*   if (UNLIKELY(cmd.offset == InvalidLPN)) {
      // This is GC write request and this request must have spare field
      auto iter = cmd.subCommandList.begin();

      iter->lpn = readSpare(iter->spare);
      LPN slpn = getSLPNfromLPN(iter->lpn);

      cmd.offset = iter->lpn;

      // Read all
      ++iter;

      for (; iter != cmd.subCommandList.end(); ++iter) {
        iter->lpn = readSpare(iter->spare);

        panic_if(slpn != getSLPNfromLPN(iter->lpn),
                 "Command has two or more superpages.");
      }
    } */

  // Perform write translation
  LPN lpn = cmd->getLPN();
  LPN lspn = lpn / param.superpage;
  LPN superpageIndex = lpn % param.superpage;
  PPN pspn = InvalidPPN;

  requestedWriteCount++;
  writeLPNCount += param.superpage;

  fstat += writeMappingInternal(lspn, pspn);

  cmd->setPPN(pspn * param.superpage + superpageIndex);

  debugprint(Log::DebugID::FTL_PageLevel,
             "Write | LPN %" PRIx64 "h -> PPN %" PRIx64 "h", lpn,
             cmd->getPPN());

  requestMemoryAccess(eid, cmd->getTag(), fstat);
}

void PageLevel::invalidateMapping(Request *cmd, Event eid) {
  // TODO: consider fullsize of request -- if fullsize is smaller than superpage
  // we should not erase mapping
  panic("Trim/Format not implemented");

  scheduleNow(eid, cmd->getTag());
}

void PageLevel::getMappingSize(uint64_t *min, uint64_t *pre) {
  if (min) {
    *min = param.superpage;
  }
  if (pre) {
    *pre = param.superpage;
  }
}

void PageLevel::getCopyList(CopyList & /* copy */, Event eid) {
  // TODO: implement GC
  panic("GC not implemented");

  scheduleNow(eid);
}

void PageLevel::releaseCopyList(CopyList &copy) {
  // Destroy all commands
  debugprint(Log::DebugID::FTL_PageLevel, "Erase | (S)PPN %" PRIx64 "h",
             copy.blockID);
}

void PageLevel::getStatList(std::vector<Stat> &list,
                            std::string prefix) noexcept {
  AbstractMapping::getStatList(list, prefix);
}

void PageLevel::getStatValues(std::vector<double> &values) noexcept {
  AbstractMapping::getStatValues(values);
}

void PageLevel::resetStatValues() noexcept {
  AbstractMapping::resetStatValues();
}

void PageLevel::createCheckpoint(std::ostream &out) const noexcept {
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

void PageLevel::restoreCheckpoint(std::istream &in) noexcept {
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
