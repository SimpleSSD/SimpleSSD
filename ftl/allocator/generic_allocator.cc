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
      mtengine(rd()) {
  selectionMode = (Config::VictimSelectionMode)readConfigUint(
      Section::FlashTranslation, Config::Key::VictimSelectionPolicy);
  dchoice =
      readConfigUint(Section::FlashTranslation, Config::Key::SamplingFactor);
  fgcThreshold = readConfigFloat(Section::FlashTranslation,
                                 Config::Key::ForegroundGCThreshold);
  bgcThreshold = readConfigFloat(Section::FlashTranslation,
                                 Config::Key::BackgroundGCThreshold);

  switch (selectionMode) {
    case Config::VictimSelectionMode::Random:
      victimSelectionFunction = [this](uint64_t idx, CopyContext &ctx) {
        return randomVictimSelection(idx, ctx);
      };

      break;
    case Config::VictimSelectionMode::Greedy:
      victimSelectionFunction = [this](uint64_t idx, CopyContext &ctx) {
        return greedyVictimSelection(idx, ctx);
      };

      break;
    case Config::VictimSelectionMode::CostBenefit:
      victimSelectionFunction = [this](uint64_t idx, CopyContext &ctx) {
        return costbenefitVictimSelection(idx, ctx);
      };

      break;
    case Config::VictimSelectionMode::DChoice:
      victimSelectionFunction = [this](uint64_t idx, CopyContext &ctx) {
        return dchoiceVictimSelection(idx, ctx);
      };

      break;
    default:
      panic("Unexpected victim block selection mode.");

      break;
  }
}

GenericAllocator::~GenericAllocator() {}

CPU::Function GenericAllocator::randomVictimSelection(uint64_t idx,
                                                      CopyContext &ctx) {
  CPU::Function fstat;
  CPU::markFunction(fstat);

  auto &currentList = sortedBlockList[idx].fullBlocks;
  std::uniform_int_distribution<uint64_t> dist(0, currentList.size() - 1);

  // Select one block from current full block list
  uint64_t ridx = dist(mtengine);

  // Get block ID from list
  auto iter = currentList.begin();

  // O(n)
  std::advance(iter, ridx);

  ctx.blockID = *iter;
  currentList.erase(iter);
  fullBlockCount--;

  return fstat;
}

CPU::Function GenericAllocator::greedyVictimSelection(uint64_t idx,
                                                      CopyContext &ctx) {
  CPU::Function fstat;
  CPU::markFunction(fstat);

  auto &currentList = sortedBlockList[idx].fullBlocks;
  auto minIndex = currentList.begin();
  uint32_t min = std::numeric_limits<uint32_t>::max();

  // Collect valid pages and find min value
  for (auto iter = currentList.begin(); iter != currentList.end(); ++iter) {
    auto valid = blockMetadata[*iter].validPages.count();

    if (min > valid) {
      min = valid;
      minIndex = iter;
    }
  }

  // Get block ID from sorted list
  ctx.blockID = *minIndex;
  currentList.erase(minIndex);
  fullBlockCount--;

  return fstat;
}

CPU::Function GenericAllocator::costbenefitVictimSelection(uint64_t idx,
                                                           CopyContext &ctx) {
  CPU::Function fstat;
  CPU::markFunction(fstat);

  const uint32_t pageCount = object.config->getNANDStructure()->page;

  auto &currentList = sortedBlockList[idx].fullBlocks;
  auto minIndex = currentList.begin();
  float min = std::numeric_limits<float>::max();

  // Collect valid pages and find min value
  for (auto iter = currentList.begin(); iter != currentList.end(); ++iter) {
    float util = (float)blockMetadata[*iter].validPages.count() / pageCount;

    util = util / ((1.f - util) * blockMetadata[*iter].insertedAt);

    if (min > util) {
      min = util;
      minIndex = iter;
    }
  }

  // Get block ID from sorted list
  ctx.blockID = *minIndex;
  currentList.erase(minIndex);
  fullBlockCount--;

  return fstat;
}

CPU::Function GenericAllocator::dchoiceVictimSelection(uint64_t idx,
                                                       CopyContext &ctx) {
  CPU::Function fstat;
  CPU::markFunction(fstat);

  auto &currentList = sortedBlockList[idx].fullBlocks;

  if (UNLIKELY(currentList.size() <= dchoice)) {
    // Just return least erased blocks
    ctx.blockID = currentList.front();

    currentList.pop_front();
    fullBlockCount--;

    return fstat;
  }

  std::uniform_int_distribution<uint64_t> dist(0, currentList.size() - 1);
  std::vector<uint64_t> offsets;
  std::vector<std::pair<std::list<PSBN>::iterator, uint32_t>> valid;

  // Select dchoice number of blocks from current full block list
  offsets.reserve(dchoice);

  while (offsets.size() < dchoice) {
    uint64_t ridx = dist(mtengine);
    bool unique = true;

    for (auto &iter : offsets) {
      if (iter == ridx) {
        unique = false;

        break;
      }
    }

    if (unique) {
      offsets.emplace_back(ridx);
    }
  }

  std::sort(offsets.begin(), offsets.end());

  // Get block ID
  auto iter = currentList.begin();

  for (uint64_t i = 0; i < currentList.size(); i++) {
    if (i == offsets.at(valid.size())) {
      valid.emplace_back(iter, blockMetadata[*iter].validPages.count());
    }

    ++iter;
  }

  // Greedy
  auto minIndex = currentList.end();
  uint32_t min = std::numeric_limits<uint32_t>::max();

  for (auto &iter : valid) {
    if (min > iter.second) {
      min = iter.second;
      minIndex = iter.first;
    }
  }

  // Get block ID from sorted list
  ctx.blockID = *minIndex;
  currentList.erase(minIndex);
  fullBlockCount--;

  return fstat;
}

void GenericAllocator::initialize() {
  AbstractAllocator::initialize();

  {
    // Special handling...
    auto &_totalSuperblock = const_cast<uint64_t &>(totalSuperblock);
    auto &_superpage = const_cast<uint32_t &>(superpage);
    auto &_parallelism = const_cast<uint32_t &>(parallelism);

    _superpage = param->superpage;
    _parallelism = param->parallelism / superpage;
    _totalSuperblock = param->totalPhysicalBlocks / superpage;
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

void GenericAllocator::getVictimBlocks(CopyContext &ctx, Event eid,
                                       uint64_t data) {
  CPU::Function fstat;

  // Select block
  fstat = victimSelectionFunction(lastErased++, ctx);

  if (lastErased == parallelism) {
    lastErased = 0;
  }

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
