// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "ftl/mapping/page_level_mapping.hh"

#include <random>

#include "ftl/allocator/abstract_allocator.hh"
#include "ftl/base/abstract_ftl.hh"

namespace SimpleSSD::FTL::Mapping {

PageLevelMapping::PageLevelMapping(ObjectData &o, FTLObjectData &fo)
    : AbstractMapping(o, fo),
      totalPhysicalSuperPages(param.totalPhysicalPages / param.superpage),
      totalPhysicalSuperBlocks(param.totalPhysicalBlocks / param.superpage),
      totalLogicalSuperPages(param.totalLogicalPages / param.superpage),
      table(nullptr) {
  // Check spare size
  panic_if(filparam->spareSize < sizeof(LPN), "NAND spare area is too small.");
}

PageLevelMapping::~PageLevelMapping() {
  free(table);
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

    auto &bmeta = getBlockMetadata(block);

    bmeta.markAsRead();
    bmeta.insertedAt = getTick();

    insertMemoryAddress(
        false, makeMetadataAddress(block) + bmeta.offsetofReadCount(), 4);
  }
  else {
    pspn.invalidate();
  }

  return fstat;
}

CPU::Function PageLevelMapping::writeMappingInternal(LSPN lspn, PSPN &pspn,
                                                     bool fixedIndex,
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
    uint32_t page = param.getPageIndexFromPSPN(old);

    auto &bmeta = getBlockMetadata(block);

    bmeta.validPages.reset(page);

    // Memory timing after demand paging
    insertMemoryAddress(false,
                        makeMetadataAddress(block) + bmeta.offsetofBitmap(page),
                        1, !init);
  }

  // Get block from allocated block pool
  uint32_t blockIndex = std::numeric_limits<uint32_t>::max();

  if (fixedIndex) {
    panic_if(!pspn.isValid(), "Invalid PPN while write.");

    auto psbn = param.getPSBNFromPSPN(pspn);
    blockIndex = param.getParallelismIndexFromPSBN(psbn);
  }

  PSBN blockID = getFreeBlockAt(blockIndex);
  auto bmeta = &getBlockMetadata(blockID);

  // Check we have to get new block
  if (bmeta->isFull()) {
    // Get a new block
    fstat += allocateFreeBlock(blockID);

    bmeta = &getBlockMetadata(blockID);

    panic_if(bmeta->isFull(), "BlockAllocator corrupted.");
  }

  // Get new page
  bmeta->validPages.set(bmeta->nextPageToWrite);
  insertMemoryAddress(false,
                      makeMetadataAddress(blockID) +
                          bmeta->offsetofBitmap(bmeta->nextPageToWrite),
                      1, !init);

  pspn = param.makePSPN(blockID, bmeta->nextPageToWrite++);

  bmeta->insertedAt = getTick();

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
    uint32_t page = param.getPageIndexFromPSPN(pspn);

    auto &bmeta = getBlockMetadata(block);

    bmeta.validPages.reset(page);
    insertMemoryAddress(
        false, makeMetadataAddress(block) + bmeta.offsetofBitmap(page), 1);

    insertMemoryAddress(false, makeTableAddress(lspn), entrySize);

    // Invalidate entry
    pspn.invalidate();

    writeTableEntry(table, lspn, 0);
  }

  return fstat;
}

void PageLevelMapping::initialize() {
  AbstractMapping::initialize();

  // Allocate table and block metadata
  entrySize = makeEntrySize(totalLogicalSuperPages, 1, readTableEntry,
                            writeTableEntry, parseTableEntry, makeTableEntry);

  table = (uint8_t *)calloc(totalLogicalSuperPages, entrySize);

  panic_if(!table, "Memory mapping for mapping table failed.");

  tableBaseAddress = object.memory->allocate(
      totalLogicalSuperPages * entrySize, Memory::MemoryType::DRAM,
      "FTL::Mapping::PageLevelMapping::Table", true);

  // Retry allocation
  tableBaseAddress = object.memory->allocate(
      totalLogicalSuperPages * entrySize, Memory::MemoryType::DRAM,
      "FTL::Mapping::PageLevelMapping::Table");

  // Memory usage information
  debugprint(Log::DebugID::FTL_PageLevel,
             "Size of mapping table: %" PRIu64 " bytes",
             totalLogicalSuperPages * entrySize);

  // Make free block pool in ftlobject.pAllocator
  for (uint64_t i = 0; i < param.parallelism; i += param.superpage) {
    PSBN tmp;

    allocateFreeBlock(tmp);
  }
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

void PageLevelMapping::writeMapping(LSPN lspn, PSPN &pspn) {
  writeMappingInternal(lspn, pspn, false, true);
}

void PageLevelMapping::writeMapping(LSPN lspn, PSPN &pspn, Event eid,
                                    uint64_t data) {
  CPU::Function fstat;
  CPU::markFunction(fstat);

  requestedWriteCount++;
  writeLPNCount += param.superpage;

  fstat += writeMappingInternal(lspn, pspn, true, false);

  debugprint(Log::DebugID::FTL_PageLevel,
             "Write | Internal | LSPN %" PRIx64 "h -> PSPN %" PRIx64 "h", lspn,
             pspn);

  requestMemoryAccess(eid, data, fstat);
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
}

}  // namespace SimpleSSD::FTL::Mapping
