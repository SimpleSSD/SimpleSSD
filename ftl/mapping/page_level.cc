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
      blockMetadata(nullptr) {
  // Check spare size
  panic_if(filparam->spareSize < sizeof(LPN), "NAND spare area is too small.");

  // Allocate table and block metadata
  entrySize = makeEntrySize();

  table = (uint8_t *)calloc(totalLogicalSuperPages, entrySize);
  blockMetadata = new BlockMetadata[totalPhysicalSuperBlocks]();

  panic_if(!table, "Memory allocation for mapping table failed.");
  panic_if(!blockMetadata, "Memory allocation for block metadata failed.");

  // Fill metadata
  for (uint64_t i = 0; i < totalPhysicalSuperBlocks; i++) {
    blockMetadata[i] = std::move(BlockMetadata(i, filparam->page));
  }
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
    PPN block = getSBFromSPPN(old);
    PPN page = getPageIndexFromSPPN(old);

    blockMetadata[block].validPages.reset(page);
  }
  else {
    validEntry.set(lpn);
  }

  // Get block from allocated block pool
  PPN idx = allocator->getBlockAt(InvalidPPN);

  auto block = &blockMetadata[idx];

  // Check we have to get new block
  if (block->nextPageToWrite == block->validPages.size()) {
    // Get a new block
    fstat += allocator->allocateBlock(idx);

    block = &blockMetadata[idx];
  }

  // Get new page
  block->validPages.set(block->nextPageToWrite);
  ppn = makeSPPN(block->blockID, block->nextPageToWrite++);

  // Write entry
  writeEntry(lpn, ppn);

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

    blockMetadata[getSBFromSPPN(old)].validPages.reset(index);
  }

  return std::move(fstat);
}

void PageLevel::makeSpare(LPN lpn, std::vector<uint8_t> &spare) {
  if (UNLIKELY(spare.size() != filparam->spareSize)) {
    spare.resize(filparam->spareSize);
  }

  memcpy(spare.data(), &lpn, sizeof(LPN));
}

LPN PageLevel::readSpare(std::vector<uint8_t> &spare) {
  LPN lpn = InvalidLPN;

  memcpy(&lpn, spare.data(), sizeof(LPN));

  return lpn;
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
  PPN ppn;
  std::vector<uint8_t> spare;

  debugprint(Log::DebugID::FTL_PageLevel, "Initialization started");

  nPagesToWarmup =
      totalLogicalSuperPages *
      readConfigFloat(Section::FlashTranslation, Config::Key::FillRatio);
  nPagesToInvalidate =
      totalLogicalSuperPages *
      readConfigFloat(Section::FlashTranslation, Config::Key::InvalidFillRatio);
  mode = (Config::FillingType)readConfigUint(Section::FlashTranslation,
                                             Config::Key::FillingMode);
  maxPagesBeforeGC =
      filparam->page * (totalPhysicalSuperBlocks *
                        (1.f - readConfigFloat(Section::FlashTranslation,
                                               Config::Key::GCThreshold)));

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
      makeSpare(i, spare);
      pFTL->writeSpare(ppn, spare);
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
      makeSpare(lpn, spare);
      pFTL->writeSpare(ppn, spare);
    }
  }

  // Step 2. Invalidating
  if (mode == Config::FillingType::SequentialSequential) {
    // Sequential
    for (uint64_t i = 0; i < nPagesToInvalidate; i++) {
      writeMappingInternal(i, ppn);
      makeSpare(i, spare);
      pFTL->writeSpare(ppn, spare);
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
      writeMappingInternal(dist(gen), ppn);
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
      makeSpare(lpn, spare);
      pFTL->writeSpare(ppn, spare);
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
  return blockMetadata[getSBFromSPPN(ppn)].validPages.count();
}

void PageLevel::readMapping(Command &cmd, Event eid) {
  CPU::Function fstat = CPU::initFunction();

  panic_if(cmd.subCommandList.size() > 0, "Unexpected sub commands.");

  // Perform read translation
  LPN lpn = InvalidLPN;
  PPN ppn = InvalidPPN;
  LPN i = cmd.offset;

  do {
    LPN currentLPN = i / param.superpage;
    LPN superpageIndex = i % param.superpage;
    PPN ippn;

    if (lpn != currentLPN) {
      lpn = currentLPN;

      readMappingInternal(lpn, ppn);

      debugprint(Log::DebugID::FTL_PageLevel,
                 "Read  | SLPN %" PRIx64 "h -> SPPN %" PRIx64 "h", lpn, ppn);
    }

    ippn = ppn * param.superpage + superpageIndex;

    debugprint(Log::DebugID::FTL_PageLevel,
               "Read  | LPN %" PRIx64 "h -> PPN %" PRIx64 "h", i, ippn);

    // Add subcommand
    commandManager->appendTranslation(cmd, i, ippn);

    i++;
  } while (i < cmd.offset + cmd.length);

  scheduleFunction(CPU::CPUGroup::FlashTranslationLayer, eid, cmd.tag, fstat);
}

void PageLevel::writeMapping(Command &cmd, Event eid) {
  CPU::Function fstat = CPU::initFunction();

  panic_if(cmd.subCommandList.size() > 0, "Unexpected sub commands.");

  // Perform write translation
  LPN lpn = InvalidLPN;
  PPN ppn = InvalidPPN;
  LPN i = cmd.offset;

  do {
    LPN currentLPN = i / param.superpage;
    LPN superpageIndex = i % param.superpage;
    PPN ippn;

    if (lpn != currentLPN) {
      lpn = currentLPN;

      fstat += writeMappingInternal(lpn, ppn);

      debugprint(Log::DebugID::FTL_PageLevel,
                 "Write | SLPN %" PRIx64 "h -> SPPN %" PRIx64 "h", lpn, ppn);
    }

    ippn = ppn * param.superpage + superpageIndex;

    debugprint(Log::DebugID::FTL_PageLevel,
               "Write | LPN %" PRIx64 "h -> PPN %" PRIx64 "h", i, ippn);

    // Add subcommand
    commandManager->appendTranslation(cmd, i, ippn);

    i++;
  } while (i < cmd.offset + cmd.length);

  scheduleFunction(CPU::CPUGroup::FlashTranslationLayer, eid, cmd.tag, fstat);
}

void PageLevel::invalidateMapping(Command &cmd, Event eid) {
  CPU::Function fstat = CPU::initFunction();

  panic_if(cmd.subCommandList.size() > 0, "Unexpected sub commands.");

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

      debugprint(Log::DebugID::FTL_PageLevel,
                 "Trim/Format | SLPN %" PRIx64 "h -> SPPN %" PRIx64 "h", lpn,
                 ppn);
    }

    ippn = ppn * param.superpage + superpageIndex;

    debugprint(Log::DebugID::FTL_PageLevel,
               "Trim/Format | LPN %" PRIx64 "h -> PPN %" PRIx64 "h", i, ippn);

    // Add subcommand
    commandManager->appendTranslation(cmd, i, ippn);

    i++;
  } while (i < cmd.offset + cmd.length);

  scheduleFunction(CPU::CPUGroup::FlashTranslationLayer, eid, cmd.tag, fstat);
}

void PageLevel::getCopyList(CopyList &copy, Event eid) {
  CPU::Function fstat = CPU::initFunction();

  panic_if(copy.blockID >= totalPhysicalSuperBlocks, "Block out of range.");

  auto block = &blockMetadata[copy.blockID];

  // Block should be full
  panic_if(block->nextPageToWrite != filparam->page,
           "Try to erase not-full block.");

  // For the valid pages in target block, create I/O operation
  copy.commandList.reserve(filparam->page);

  for (uint32_t i = 0; i < filparam->page; i++) {
    if (block->validPages.test(i)) {
      uint64_t tag = pFTL->makeFTLCommandTag();
      auto &copycmd = commandManager->createFTLCommand(tag);

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

  validEntry.createCheckpoint(out);

  for (uint64_t i = 0; i < totalPhysicalSuperBlocks; i++) {
    BACKUP_SCALAR(out, blockMetadata[i].nextPageToWrite);

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

  validEntry.restoreCheckpoint(in);

  for (uint64_t i = 0; i < totalPhysicalSuperBlocks; i++) {
    RESTORE_SCALAR(in, blockMetadata[i].nextPageToWrite);

    blockMetadata[i].validPages.restoreCheckpoint(in);
  }
}

}  // namespace SimpleSSD::FTL::Mapping
