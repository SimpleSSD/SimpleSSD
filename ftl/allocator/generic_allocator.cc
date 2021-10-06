// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 *         Junhyeok Jang <jhjang@camelab.org>
 */

#include "ftl/allocator/generic_allocator.hh"

#include "ftl/mapping/abstract_mapping.hh"

namespace SimpleSSD::FTL::BlockAllocator {

GenericAllocator::GenericAllocator(ObjectData &o, FTLObjectData &fo)
    : AbstractAllocator(o, fo),
      totalSuperblock(0),
      superpage(0),
      parallelism(0),
      metadataBaseAddress(0),
      metadataEntrySize(0) {
  fgcThreshold = readConfigFloat(Section::FlashTranslation,
                                 Config::Key::ForegroundGCThreshold);
  bgcThreshold = readConfigFloat(Section::FlashTranslation,
                                 Config::Key::BackgroundGCThreshold);
}

GenericAllocator::~GenericAllocator() {}

void GenericAllocator::initialize() {
  AbstractAllocator::initialize();

  {
    auto &_totalSuperblock = const_cast<uint64_t &>(totalSuperblock);
    auto &_superpage = const_cast<uint32_t &>(superpage);
    auto &_parallelism = const_cast<uint32_t &>(parallelism);

    _superpage = param->superpage;
    _parallelism = param->parallelism / superpage;
    _totalSuperblock = param->totalPhysicalBlocks / superpage;
  }

  {
    auto &_addr = const_cast<uint64_t &>(metadataBaseAddress);
    auto &_size = const_cast<uint64_t &>(metadataEntrySize);

    auto filparam = object.config->getNANDStructure();

    _size = BlockMetadata::sizeofMetadata(filparam->page);
    _addr = object.memory->allocate(
        totalSuperblock * metadataEntrySize, Memory::MemoryType::DRAM,
        "FTL::Mapping::PageLevelMapping::BlockMeta");

    blockMetadata.resize(totalSuperblock);

    for (uint64_t i = 0; i < totalSuperblock; i++) {
      blockMetadata[i] = BlockMetadata(filparam->page);
    }
  }

  freeBlockCount = totalSuperblock;
  fullBlockCount = 0;

  if ((float)parallelism / totalSuperblock * 2.f >= fgcThreshold) {
    warn("GC threshold cannot hold minimum blocks. Adjust threshold.");

    fgcThreshold = (float)(parallelism + 1) / totalSuperblock * 2.f;
  }

  // Allocate data
  sortedBlockList.resize(parallelism);

  lastErased = 0;
  lastAllocated = 0;

  // Fill data
  uint64_t left = totalSuperblock / parallelism;

  for (uint32_t i = 0; i < parallelism; i++) {
    for (uint64_t j = 0; j < left; j++) {
      sortedBlockList[i].freeBlocks.emplace_back(i + j * parallelism);
    }
  }
}

CPU::Function GenericAllocator::allocateFreeBlock(PSBN &blockUsed) {
  CPU::Function fstat;
  CPU::markFunction(fstat);

  AllocationMetadata *ameta = nullptr;
  uint64_t idx = lastAllocated;

  if (LIKELY(blockUsed.isValid())) {
    idx = param->getParallelismIndexFromPSBN(blockUsed);

    ameta = &sortedBlockList[idx];

    panic_if(ameta->inUse != blockUsed, "Unexpected block ID.");

    auto &bmeta = blockMetadata[blockUsed];

    // Insert to full block list
    auto iter = ameta->fullBlocks.begin();

    for (; iter != ameta->fullBlocks.end(); ++iter) {
      if (blockMetadata[*iter].erasedCount > bmeta.erasedCount) {
        break;
      }
    }

    ameta->fullBlocks.emplace(iter, blockUsed);
    fullBlockCount++;
  }
  else {
    lastAllocated++;

    if (lastAllocated == parallelism) {
      lastAllocated = 0;
    }
  }

  ameta = &sortedBlockList[idx];

  panic_if(ameta->freeBlocks.size() == 0, "No more free blocks at ID %" PRIu64,
           idx);

  // Allocate new block
  ameta->inUse = ameta->freeBlocks.front();
  blockUsed = ameta->inUse;

  ameta->freeBlocks.pop_front();
  freeBlockCount--;

  return fstat;
}

PSBN GenericAllocator::getFreeBlockAt(uint32_t idx) noexcept {
  if (idx >= parallelism) {
    auto psbn = sortedBlockList[lastAllocated++].inUse;

    if (lastAllocated == parallelism) {
      lastAllocated = 0;
    }

    return psbn;
  }

  panic_if(idx >= parallelism, "Invalid parallelism index.");

  return sortedBlockList[idx].inUse;
}

bool GenericAllocator::checkForegroundGCThreshold() noexcept {
  return (float)freeBlockCount / totalSuperblock < fgcThreshold;
}

bool GenericAllocator::checkBackgroundGCThreshold() noexcept {
  return (float)freeBlockCount / totalSuperblock < bgcThreshold;
}

void GenericAllocator::getVictimBlocks(CopyContext &ctx,
                                       AbstractVictimSelection *method,
                                       Event eid, uint64_t data) {
  CPU::Function fstat;
  auto &ameta = sortedBlockList[lastErased];
  std::list<PSBN>::iterator iter;

  // Select block
  fstat = method->getVictim(lastErased++, iter);

  if (lastErased == parallelism) {
    lastErased = 0;
  }

  // Erase block from full block pool
  ctx.blockID = *iter;
  ameta.fullBlocks.erase(iter);
  fullBlockCount--;

  // Fill copy context
  const auto &bmeta = blockMetadata[ctx.blockID];

  for (uint32_t i = 0; i < bmeta.validPages.size(); i++) {
    if (bmeta.validPages.test(i)) {
      ctx.copyList.emplace_back(i);
    }
  }

  scheduleFunction(CPU::CPUGroup::FlashTranslationLayer, eid, data, fstat);
}

void GenericAllocator::reclaimBlocks(PSBN blockID, Event eid, uint64_t data) {
  CPU::Function fstat;
  CPU::markFunction(fstat);

  panic_if(blockID >= totalSuperblock, "Invalid block ID.");

  // Insert PPN to free block list
  uint32_t idx = param->getParallelismIndexFromPSBN(blockID);

  auto &ameta = sortedBlockList[idx];
  auto &bmeta = blockMetadata[blockID];

  bmeta.markAsErased();

  auto iter = ameta.freeBlocks.begin();

  for (; iter != ameta.freeBlocks.end(); ++iter) {
    if (blockMetadata[*iter].erasedCount > bmeta.erasedCount) {
      break;
    }
  }

  ameta.freeBlocks.emplace(iter, blockID);
  freeBlockCount++;

  scheduleFunction(CPU::CPUGroup::FlashTranslationLayer, eid, data, fstat);
}

void GenericAllocator::getPageStatistics(uint64_t &valid,
                                         uint64_t &invalid) noexcept {
  valid = 0;
  invalid = 0;

  for (uint64_t i = 0; i < totalSuperblock; i++) {
    auto &block = blockMetadata[i];

    if (block.nextPageToWrite > 0) {
      valid += block.validPages.count();
      invalid += block.nextPageToWrite - valid;
    }
  }
}

std::list<PSBN> &GenericAllocator::getBlockListAtParallelismIndex(
    uint32_t idx) noexcept {
  panic_if(idx >= parallelism, "Parallelism index out-of-range.");

  return sortedBlockList[idx].fullBlocks;
}

void GenericAllocator::getStatList(std::vector<Stat> &list,
                                   std::string prefix) noexcept {
  list.emplace_back(prefix + "wear_leveling.factor", "Wear-leveling factor");
  list.emplace_back(prefix + "erasecount.min", "Minimum block erased count.");
  list.emplace_back(prefix + "erasecount.average",
                    "Average block erased count.");
  list.emplace_back(prefix + "erasecount.max", "Maximum block erased count.");
  list.emplace_back(prefix + "freeblock.count",
                    "Total number of free/clean (super)blocks");
  list.emplace_back(prefix + "freeblock.ratio",
                    "Ratio of free (super)block / total (super)blocks");
  list.emplace_back(prefix + "fullblock.count",
                    "Total number of full/closed (super)blocks");
  list.emplace_back(prefix + "inuseblock.count",
                    "Total number of inuse/open (super)blocks");
}

void GenericAllocator::getStatValues(std::vector<double> &values) noexcept {
  double total = 0.;
  double square = 0.;
  double result = 0.;
  uint32_t min = std::numeric_limits<uint32_t>::max();
  uint32_t max = 0;

  // TODO: Do I need to speed-up this function?
  for (uint64_t i = 0; i < totalSuperblock; i++) {
    auto erased = blockMetadata[i].erasedCount;

    total += (double)erased;
    square += pow((double)erased, 2.);
    min = MIN(min, erased);
    max = MAX(max, erased);
  }

  if (square > 0.) {
    result = total * total / square / (double)totalSuperblock;
  }

  values.push_back(result);
  values.push_back((double)min);
  values.push_back(total / totalSuperblock);
  values.push_back((double)max);
  values.push_back((double)freeBlockCount);
  values.push_back((double)freeBlockCount / totalSuperblock);
  values.push_back((double)fullBlockCount);
  values.push_back((double)(totalSuperblock - freeBlockCount - fullBlockCount));
}

void GenericAllocator::resetStatValues() noexcept {}

void GenericAllocator::createCheckpoint(std::ostream &out) const noexcept {
  BACKUP_SCALAR(out, totalSuperblock);
  BACKUP_SCALAR(out, parallelism);
  BACKUP_SCALAR(out, lastErased);
  BACKUP_SCALAR(out, lastAllocated);
  BACKUP_SCALAR(out, freeBlockCount);
  BACKUP_SCALAR(out, fullBlockCount);

  BACKUP_STL(out, blockMetadata, iter, { iter.createCheckpoint(out); });
  BACKUP_STL(out, sortedBlockList, iter, { iter.createCheckpoint(out); });
}

void GenericAllocator::restoreCheckpoint(std::istream &in) noexcept {
  uint32_t tmp32;
  uint64_t tmp64;

  RESTORE_SCALAR(in, tmp64);
  panic_if(tmp64 != totalSuperblock, "FTL configuration mismatch.");

  RESTORE_SCALAR(in, tmp32);
  panic_if(tmp32 != parallelism, "FTL configuration mismatch.");

  RESTORE_SCALAR(in, lastErased);
  RESTORE_SCALAR(in, lastAllocated);
  RESTORE_SCALAR(in, freeBlockCount);
  RESTORE_SCALAR(in, fullBlockCount);

  RESTORE_STL_RESIZE(in, blockMetadata, iter, { iter.restoreCheckpoint(in); });
  RESTORE_STL_RESIZE(in, sortedBlockList, iter, { iter.restoreCheckpoint(in); })
}

}  // namespace SimpleSSD::FTL::BlockAllocator
