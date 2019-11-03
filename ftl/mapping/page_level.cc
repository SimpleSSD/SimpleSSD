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

PageLevel::PageLevel(ObjectData &o, CommandManager *c)
    : AbstractMapping(o, c),
      totalPhysicalSuperPages(param.totalPhysicalPages / param.superpage),
      totalPhysicalSuperBlocks(param.totalPhysicalBlocks / param.superpage),
      totalLogicalSuperPages(param.totalLogicalPages / param.superpage),
      table(nullptr),
      validEntry(totalLogicalSuperPages),
      blockMetadata(nullptr),
      clock(0) {
  // Check spare size
  panic_if(filparam->spareSize < sizeof(LPN), "NAND spare area is too small.");

  // Allocate table and block metadata
  entrySize = makeEntrySize();

  table = (uint8_t *)calloc(totalLogicalSuperPages, entrySize);
  blockMetadata = new BlockMetadata[totalPhysicalSuperBlocks]();

  panic_if(!table, "Memory allocation for mapping table failed.");
  panic_if(!blockMetadata, "Memory allocation for block metadata failed.");

  tableBaseAddress = object.dram->allocate(totalLogicalSuperPages * entrySize,
                                           "FTL::Mapping::PageLevel::Table");

  // Fill metadata
  for (uint64_t i = 0; i < totalPhysicalSuperBlocks; i++) {
    blockMetadata[i] = std::move(BlockMetadata(i, filparam->page));
  }

  // Valid page bits (packed) + 2byte clock + 2byte page offset
  metadataEntrySize = DIVCEIL(filparam->page, 8) + 4;

  metadataBaseAddress =
      object.dram->allocate(totalPhysicalSuperBlocks * metadataEntrySize,
                            "FTL::Mapping::PageLevel::BlockMeta");
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

CPU::Function PageLevel::readMappingInternal(LPN lpn, PPN &ppn) {
  CPU::Function fstat = CPU::initFunction();

  panic_if(lpn >= totalLogicalSuperPages, "LPN out of range.");

  if (validEntry.test(lpn)) {
    ppn = readEntry(lpn);
    insertMemoryAddress(true, tableBaseAddress + lpn * entrySize, entrySize);

    // Update accessed time
    PPN block = getSBFromSPPN(ppn);
    blockMetadata[block].clock = clock;
    insertMemoryAddress(false, metadataBaseAddress + block * metadataEntrySize,
                        2);
  }
  else {
    ppn = InvalidPPN;
  }

  return std::move(fstat);
}

CPU::Function PageLevel::writeMappingInternal(LPN lpn, PPN &ppn) {
  CPU::Function fstat = CPU::initFunction();

  panic_if(lpn >= totalLogicalSuperPages, "LPN out of range.");

  if (validEntry.test(lpn)) {
    // This is valid entry, invalidate block
    PPN old = readEntry(lpn);
    insertMemoryAddress(true, tableBaseAddress + lpn * entrySize, entrySize);

    PPN block = getSBFromSPPN(old);
    PPN page = getPageIndexFromSPPN(old);

    blockMetadata[block].validPages.reset(page);
    insertMemoryAddress(
        false, metadataBaseAddress + block * metadataEntrySize + 4 + page / 8,
        1);
  }
  else {
    validEntry.set(lpn);
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
  insertMemoryAddress(false,
                      metadataBaseAddress + block->blockID * metadataEntrySize +
                          4 + block->nextPageToWrite / 8,
                      1);

  ppn = makeSPPN(block->blockID, block->nextPageToWrite++);

  block->clock = clock;
  insertMemoryAddress(
      false, metadataBaseAddress + block->blockID * metadataEntrySize, 4);

  // Write entry
  writeEntry(lpn, ppn);
  insertMemoryAddress(false, tableBaseAddress + lpn * entrySize, entrySize);

  return std::move(fstat);
}

CPU::Function PageLevel::invalidateMappingInternal(LPN lpn, PPN &old) {
  CPU::Function fstat = CPU::initFunction();

  panic_if(lpn >= totalLogicalSuperPages, "LPN out of range.");

  if (validEntry.test(lpn)) {
    // Invalidate entry
    validEntry.reset(lpn);

    // Invalidate block
    old = readEntry(lpn);
    PPN index = getPageIndexFromSPPN(old);
    PPN block = getSBFromSPPN(old);

    blockMetadata[block].validPages.reset(index);
    insertMemoryAddress(
        false, metadataBaseAddress + block * metadataEntrySize + 4 + index / 8,
        1);
  }

  return std::move(fstat);
}

void PageLevel::initialize(AbstractFTL *f,
                           BlockAllocator::AbstractAllocator *a) {
  AbstractMapping::initialize(f, a);

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
      writeMappingInternal(i, ppn);

      for (uint32_t j = 0; j < param.superpage; j++) {
        _lpn = i * param.superpage + j;

        makeSpare(_lpn, spare);
        pFTL->writeSpare(ppn * param.superpage + j, spare);
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

      writeMappingInternal(lpn, ppn);

      for (uint32_t j = 0; j < param.superpage; j++) {
        _lpn = lpn * param.superpage + j;

        makeSpare(_lpn, spare);
        pFTL->writeSpare(ppn * param.superpage + j, spare);
      }
    }
  }

  // Step 2. Invalidating
  if (mode == Config::FillingType::SequentialSequential) {
    // Sequential
    for (uint64_t i = 0; i < nPagesToInvalidate; i++) {
      writeMappingInternal(i, ppn);

      for (uint32_t j = 0; j < param.superpage; j++) {
        _lpn = i * param.superpage + j;

        makeSpare(_lpn, spare);
        pFTL->writeSpare(ppn * param.superpage + j, spare);
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

      writeMappingInternal(lpn, ppn);

      for (uint32_t j = 0; j < param.superpage; j++) {
        _lpn = lpn * param.superpage + j;

        makeSpare(_lpn, spare);
        pFTL->writeSpare(ppn * param.superpage + j, spare);
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

      writeMappingInternal(lpn, ppn);

      for (uint32_t j = 0; j < param.superpage; j++) {
        _lpn = lpn * param.superpage + j;

        makeSpare(_lpn, spare);
        pFTL->writeSpare(ppn * param.superpage + j, spare);
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
    if (validEntry.test(i)) {
      count++;
    }
  }

  // Convert to LPN
  return count * param.superpage;
}

uint32_t PageLevel::getValidPages(PPN ppn) {
  return (uint32_t)blockMetadata[getSBFromSPPN(ppn)].validPages.count();
}

uint16_t PageLevel::getAge(PPN ppn) {
  return (uint16_t)(clock - blockMetadata[getSBFromSPPN(ppn)].clock);
}

CPU::Function PageLevel::readMapping(Command &cmd) {
  CPU::Function fstat = CPU::initFunction();

  clock++;

  panic_if(cmd.subCommandList.size() != cmd.length, "Unexpected sub commands.");

  // Perform read translation
  LPN lpn = InvalidLPN;
  PPN ppn = InvalidPPN;

  for (auto &scmd : cmd.subCommandList) {
    if (UNLIKELY(scmd.lpn == InvalidLPN)) {
      continue;
    }

    LPN currentLPN = scmd.lpn / param.superpage;
    LPN superpageIndex = scmd.lpn % param.superpage;

    if (lpn != currentLPN) {
      lpn = currentLPN;

      fstat += readMappingInternal(lpn, ppn);

      if (param.superpage != 1) {
        debugprint(Log::DebugID::FTL_PageLevel,
                   "Read  | SLPN %" PRIx64 "h -> SPPN %" PRIx64 "h", lpn, ppn);
      }
    }

    scmd.ppn = ppn * param.superpage + superpageIndex;

    debugprint(Log::DebugID::FTL_PageLevel,
               "Read  | LPN %" PRIx64 "h -> PPN %" PRIx64 "h", scmd.lpn,
               scmd.ppn);
  }

  return std::move(fstat);
}

CPU::Function PageLevel::writeMapping(Command &cmd) {
  CPU::Function fstat = CPU::initFunction();

  clock++;

  panic_if(cmd.subCommandList.size() != cmd.length, "Unexpected sub commands.");

  // Check command
  if (UNLIKELY(cmd.offset == InvalidLPN)) {
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
  }

  // Perform write translation
  LPN lpn = InvalidLPN;
  PPN ppn = InvalidPPN;

  for (auto &scmd : cmd.subCommandList) {
    LPN currentLPN = scmd.lpn / param.superpage;
    LPN superpageIndex = scmd.lpn % param.superpage;

    if (lpn != currentLPN) {
      lpn = currentLPN;

      fstat += writeMappingInternal(lpn, ppn);

      if (param.superpage != 1) {
        debugprint(Log::DebugID::FTL_PageLevel,
                   "Write | SLPN %" PRIx64 "h -> SPPN %" PRIx64 "h", lpn, ppn);
      }
    }

    scmd.ppn = ppn * param.superpage + superpageIndex;
    makeSpare(scmd.lpn, scmd.spare);

    debugprint(Log::DebugID::FTL_PageLevel,
               "Write | LPN %" PRIx64 "h -> PPN %" PRIx64 "h", scmd.lpn,
               scmd.ppn);
  }

  return std::move(fstat);
}

CPU::Function PageLevel::invalidateMapping(Command &cmd) {
  CPU::Function fstat = CPU::initFunction();

  clock++;

  panic_if(cmd.subCommandList.size() > 0, "Unexpected sub commands.");

  cmd.subCommandList.reserve(cmd.length);

  // Perform invalidation
  LPN lpn = InvalidLPN;
  PPN ppn = InvalidPPN;
  LPN i = cmd.offset;

  do {
    LPN currentLPN = i / param.superpage;
    LPN superpageIndex = i % param.superpage;
    PPN ippn;

    if (lpn != currentLPN) {
      lpn = currentLPN;

      fstat += invalidateMappingInternal(lpn, ppn);

      if (param.superpage != 1) {
        debugprint(Log::DebugID::FTL_PageLevel,
                   "Trim/Format | SLPN %" PRIx64 "h -> SPPN %" PRIx64 "h", lpn,
                   ppn);
      }
    }

    ippn = ppn * param.superpage + superpageIndex;

    debugprint(Log::DebugID::FTL_PageLevel,
               "Trim/Format | LPN %" PRIx64 "h -> PPN %" PRIx64 "h", i, ippn);

    // Add subcommand
    commandManager->appendTranslation(cmd, i, ippn);

    i++;
  } while (i < cmd.offset + cmd.length);

  return std::move(fstat);
}

void PageLevel::getCopyList(CopyList &copy, Event eid) {
  CPU::Function fstat = CPU::initFunction();

  panic_if(copy.blockID >= totalPhysicalSuperBlocks, "Block out of range.");

  auto block = &blockMetadata[copy.blockID];

  // Block should be full
  panic_if(block->nextPageToWrite != filparam->page,
           "Try to erase not-full block.");

  // For the valid pages in target block, create I/O operation
  copy.commandList.reserve(block->validPages.count());

  for (uint32_t i = 0; i < filparam->page; i++) {
    if (block->validPages.test(i)) {
      uint64_t tag = pFTL->makeFTLCommandTag();
      auto &copycmd = commandManager->createFTLCommand(tag);

      copycmd.offset = InvalidLPN;  // writeMapping will fill this
      copycmd.length = param.superpage;

      // At this stage, we don't know LPN
      for (uint64_t j = 0; j < param.superpage; j++) {
        commandManager->appendTranslation(copycmd, InvalidLPN,
                                          makePPN(copy.blockID, j, i));
      }

      copy.commandList.emplace_back(tag);
    }
  }

  // For the target block, create erase operation
  copy.eraseTag = pFTL->makeFTLCommandTag();

  auto &erasecmd = commandManager->createFTLCommand(copy.eraseTag);

  erasecmd.offset = InvalidLPN;  // Erase has no LPN
  erasecmd.length = param.superpage;

  for (uint32_t i = 0; i < param.superpage; i++) {
    commandManager->appendTranslation(erasecmd, InvalidLPN,
                                      makePPN(copy.blockID, i, 0));
  }

  scheduleFunction(CPU::CPUGroup::FlashTranslationLayer, eid, fstat);
}

void PageLevel::releaseCopyList(CopyList &copy) {
  // Destroy all commands
  for (auto &iter : copy.commandList) {
    commandManager->destroyCommand(iter);
  }

  commandManager->destroyCommand(copy.eraseTag);

  // Mark block as erased
  blockMetadata[copy.blockID].nextPageToWrite = 0;
  blockMetadata[copy.blockID].validPages.reset();

  debugprint(Log::DebugID::FTL_PageLevel, "Erase | (S)PPN %" PRIx64 "h",
             copy.blockID);
}

void PageLevel::getStatList(std::vector<Stat> &, std::string) noexcept {}

void PageLevel::getStatValues(std::vector<double> &) noexcept {}

void PageLevel::resetStatValues() noexcept {}

void PageLevel::createCheckpoint(std::ostream &out) const noexcept {
  BACKUP_SCALAR(out, totalPhysicalSuperPages);
  BACKUP_SCALAR(out, totalPhysicalSuperBlocks);
  BACKUP_SCALAR(out, totalLogicalSuperPages);
  BACKUP_SCALAR(out, entrySize);
  BACKUP_BLOB(out, table, totalLogicalSuperPages * entrySize);
  BACKUP_SCALAR(out, clock);

  validEntry.createCheckpoint(out);

  for (uint64_t i = 0; i < totalPhysicalSuperBlocks; i++) {
    BACKUP_SCALAR(out, blockMetadata[i].nextPageToWrite);
    BACKUP_SCALAR(out, blockMetadata[i].clock);

    blockMetadata[i].validPages.createCheckpoint(out);
  }
}

void PageLevel::restoreCheckpoint(std::istream &in) noexcept {
  uint64_t tmp64;

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

  RESTORE_BLOB(in, table, totalLogicalSuperPages * entrySize);
  RESTORE_SCALAR(in, clock);

  validEntry.restoreCheckpoint(in);

  for (uint64_t i = 0; i < totalPhysicalSuperBlocks; i++) {
    RESTORE_SCALAR(in, blockMetadata[i].nextPageToWrite);
    RESTORE_SCALAR(in, blockMetadata[i].clock);

    blockMetadata[i].validPages.restoreCheckpoint(in);
  }
}

}  // namespace SimpleSSD::FTL::Mapping
