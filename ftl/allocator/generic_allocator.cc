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
      inUseBlockMap(nullptr),
      freeBlocks(nullptr),
      mtengine(rd()) {
  selectionMode = (Config::VictimSelectionMode)readConfigUint(
      Section::FlashTranslation, Config::Key::VictimSelectionPolicy);
  dchoice =
      readConfigUint(Section::FlashTranslation, Config::Key::SamplingFactor);
  fgcThreshold =
      readConfigFloat(Section::FlashTranslation, Config::Key::FGCThreshold);
  bgcThreshold =
      readConfigFloat(Section::FlashTranslation, Config::Key::BGCThreshold);

  switch (selectionMode) {
    case Config::VictimSelectionMode::Random:
      victimSelectionFunction = [this](uint64_t idx, std::vector<PSBN> &list) {
        return randomVictimSelection(idx, list);
      };

      break;
    case Config::VictimSelectionMode::Greedy:
      victimSelectionFunction = [this](uint64_t idx, std::vector<PSBN> &list) {
        return greedyVictimSelection(idx, list);
      };

      break;
    case Config::VictimSelectionMode::CostBenefit:
      victimSelectionFunction = [this](uint64_t idx, std::vector<PSBN> &list) {
        return costbenefitVictimSelection(idx, list);
      };

      break;
    case Config::VictimSelectionMode::DChoice:
      victimSelectionFunction = [this](uint64_t idx, std::vector<PSBN> &list) {
        return dchoiceVictimSelection(idx, list);
      };

      break;
    default:
      panic("Unexpected victim block selection mode.");

      break;
  }
}

GenericAllocator::~GenericAllocator() {
  delete[] eraseCountList;
  delete[] inUseBlockMap;
  delete[] freeBlocks;
  delete[] fullBlocks;
}

CPU::Function GenericAllocator::randomVictimSelection(uint64_t idx,
                                                      std::vector<PSBN> &list) {
  CPU::Function fstat;
  CPU::markFunction(fstat);

  auto &currentList = fullBlocks[idx];
  std::uniform_int_distribution<uint64_t> dist(0, currentList.size() - 1);

  // Select one block from current full block list
  uint64_t ridx = dist(mtengine);

  // Get block ID from list
  auto iter = currentList.begin();

  // O(n)
  std::advance(iter, ridx);

  list.emplace_back(*iter);
  currentList.erase(iter);
  fullBlockCount--;

  return fstat;
}

CPU::Function GenericAllocator::greedyVictimSelection(uint64_t idx,
                                                      std::vector<PSBN> &list) {
  CPU::Function fstat;
  CPU::markFunction(fstat);

  auto &currentList = fullBlocks[idx];
  std::vector<std::pair<std::list<PSBN>::iterator, uint32_t>> valid;

  // Collect valid pages
  for (auto iter = currentList.begin(); iter != currentList.end(); ++iter) {
    valid.emplace_back(iter, ftlobject.pMapping->getValidPages(*iter));
  }

  // Find min value
  auto minIndex = currentList.end();
  uint32_t min = std::numeric_limits<uint32_t>::max();

  for (auto &iter : valid) {
    if (min > iter.second) {
      min = iter.second;
      minIndex = iter.first;
    }
  }

  // Get block ID from sorted list
  list.emplace_back(*minIndex);
  currentList.erase(minIndex);
  fullBlockCount--;

  return fstat;
}

CPU::Function GenericAllocator::costbenefitVictimSelection(
    uint64_t idx, std::vector<PSBN> &list) {
  CPU::Function fstat;
  CPU::markFunction(fstat);

  const uint32_t pageCount = object.config->getNANDStructure()->page;

  auto &currentList = fullBlocks[idx];
  std::vector<std::pair<std::list<PSBN>::iterator, float>> valid;

  // Collect valid pages
  for (auto iter = currentList.begin(); iter != currentList.end(); ++iter) {
    float util = ftlobject.pMapping->getValidPages(*iter) / pageCount;

    util = util / ((1.f - util) * ftlobject.pMapping->getAge(*iter));

    valid.emplace_back(iter, util);
  }

  // Find min value
  auto minIndex = currentList.end();
  float min = std::numeric_limits<float>::max();

  for (auto &iter : valid) {
    if (min > iter.second) {
      min = iter.second;
      minIndex = iter.first;
    }
  }

  // Get block ID from sorted list
  list.emplace_back(*minIndex);
  currentList.erase(minIndex);
  fullBlockCount--;

  return fstat;
}

CPU::Function GenericAllocator::dchoiceVictimSelection(
    uint64_t idx, std::vector<PSBN> &list) {
  CPU::Function fstat;
  CPU::markFunction(fstat);

  auto &currentList = fullBlocks[idx];

  if (UNLIKELY(currentList.size() <= dchoice)) {
    // Just return least erased blocks
    list.emplace_back(currentList.front());

    currentList.pop_front();
    fullBlockCount--;
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
      valid.emplace_back(iter, ftlobject.pMapping->getValidPages(*iter));
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
  list.emplace_back(*minIndex);
  currentList.erase(minIndex);
  fullBlockCount--;

  return fstat;
}

void GenericAllocator::initialize() {
  AbstractAllocator::initialize();

  superpage = param->superpage;
  parallelism = param->parallelism / superpage;
  totalSuperblock = param->totalPhysicalBlocks / superpage;
  freeBlockCount = totalSuperblock;
  fullBlockCount = 0;

  if ((float)parallelism / totalSuperblock * 2.f >= fgcThreshold) {
    warn("GC threshold cannot hold minimum blocks. Adjust threshold.");

    fgcThreshold = (float)(parallelism + 1) / totalSuperblock * 2.f;
  }

  // Allocate data
  eraseCountList = new uint32_t[totalSuperblock]();
  inUseBlockMap = new PSBN[parallelism]();
  freeBlocks = new std::list<PSBN>[parallelism]();
  fullBlocks = new std::list<PSBN>[parallelism]();

  lastAllocated = 0;

  // Fill data
  uint64_t left = totalSuperblock / parallelism;

  for (uint64_t i = 0; i < parallelism; i++) {
    for (uint64_t j = 0; j < left; j++) {
      freeBlocks[i].emplace_back(i + j * parallelism);
    }
  }
}

CPU::Function GenericAllocator::allocateBlock(PSBN &blockUsed) {
  CPU::Function fstat;
  CPU::markFunction(fstat);

  uint64_t idx = lastAllocated;

  if (LIKELY(blockUsed.isValid())) {
    idx = param->getParallelismIndexFromPSBN(blockUsed);

    panic_if(inUseBlockMap[idx] != blockUsed, "Unexpected block ID.");

    // Insert to full block list
    uint32_t erased = eraseCountList[blockUsed];
    auto iter = fullBlocks[idx].begin();

    for (; iter != fullBlocks[idx].end(); ++iter) {
      if (eraseCountList[*iter] > erased) {
        break;
      }
    }

    fullBlocks[idx].emplace(iter, blockUsed);
    fullBlockCount++;
  }
  else {
    lastAllocated++;

    if (lastAllocated == parallelism) {
      lastAllocated = 0;
    }
  }

  panic_if(freeBlocks[idx].size() == 0, "No more free blocks at ID %" PRIu64,
           idx);

  // Allocate new block
  inUseBlockMap[idx] = freeBlocks[idx].front();
  blockUsed = inUseBlockMap[idx];

  freeBlocks[idx].pop_front();
  freeBlockCount--;

  return fstat;
}

PSBN GenericAllocator::getBlockAt(uint32_t idx) {
  if (idx >= parallelism) {
    auto psbn = inUseBlockMap[lastAllocated++];

    if (lastAllocated == parallelism) {
      lastAllocated = 0;
    }

    return psbn;
  }

  panic_if(idx >= parallelism, "Invalid parallelism index.");

  return inUseBlockMap[idx];
}

bool GenericAllocator::checkForegroundGCThreshold() {
  return (float)freeBlockCount / totalSuperblock < fgcThreshold;
}

bool GenericAllocator::checkBackgroundGCThreshold() {
  return (float)freeBlockCount / totalSuperblock < bgcThreshold;
}

bool GenericAllocator::checkFreeBlockExist() {
  return freeBlockCount > parallelism;
}

void GenericAllocator::getVictimBlocks(std::vector<PSBN> &victimList,
                                       Event eid) {
  CPU::Function fstat;
  CPU::markFunction(fstat);

  // Select victim blocks
  victimList.clear();
  victimList.reserve(parallelism);

  for (uint64_t i = 0; i < parallelism; i++) {
    fstat += victimSelectionFunction(i, victimList);
  }

  scheduleFunction(CPU::CPUGroup::FlashTranslationLayer, eid, fstat);
}

void GenericAllocator::reclaimBlocks(PSBN blockID, Event eid) {
  CPU::Function fstat;
  CPU::markFunction(fstat);

  panic_if(blockID >= totalSuperblock, "Invalid block ID.");

  // Insert PPN to free block list
  uint32_t idx = param->getParallelismIndexFromPSBN(blockID);
  uint32_t erased = ++eraseCountList[blockID];
  auto iter = freeBlocks[idx].begin();

  for (; iter != freeBlocks[idx].end(); ++iter) {
    if (eraseCountList[*iter] > erased) {
      break;
    }
  }

  freeBlocks[idx].emplace(iter, blockID);
  freeBlockCount++;

  scheduleFunction(CPU::CPUGroup::FlashTranslationLayer, eid, fstat);
}

void GenericAllocator::getStatList(std::vector<Stat> &list,
                                   std::string prefix) noexcept {
  list.emplace_back(prefix + "wear_leveling", "Wear-leveling factor");
}

void GenericAllocator::getStatValues(std::vector<double> &values) noexcept {
  double total = 0.;
  double square = 0.;
  double result = 0.;

  for (uint64_t i = 0; i < totalSuperblock; i++) {
    total += (double)eraseCountList[i];
    square += pow((double)eraseCountList[i], 2.);
  }

  if (square > 0.) {
    result = total * total / square / (double)totalSuperblock;
  }

  values.push_back(result);
}

void GenericAllocator::resetStatValues() noexcept {}

void GenericAllocator::createCheckpoint(std::ostream &out) const noexcept {
  BACKUP_SCALAR(out, parallelism);
  BACKUP_SCALAR(out, totalSuperblock);
  BACKUP_SCALAR(out, lastAllocated);
  BACKUP_BLOB(out, eraseCountList, sizeof(uint32_t) * totalSuperblock);
  BACKUP_BLOB(out, inUseBlockMap, sizeof(PPN) * parallelism);
  BACKUP_SCALAR(out, freeBlockCount);

  uint64_t size;

  for (uint64_t i = 0; i < parallelism; i++) {
    size = freeBlocks[i].size();
    BACKUP_SCALAR(out, size);

    for (auto &iter : freeBlocks[i]) {
      BACKUP_SCALAR(out, iter);
    }
  }

  for (uint64_t i = 0; i < parallelism; i++) {
    size = fullBlocks[i].size();
    BACKUP_SCALAR(out, size);

    for (auto &iter : fullBlocks[i]) {
      BACKUP_SCALAR(out, iter);
    }
  }

  BACKUP_SCALAR(out, selectionMode);
  BACKUP_SCALAR(out, fgcThreshold);
  BACKUP_SCALAR(out, bgcThreshold);
  BACKUP_SCALAR(out, dchoice);
}

void GenericAllocator::restoreCheckpoint(std::istream &in) noexcept {
  uint64_t tmp64;

  RESTORE_SCALAR(in, tmp64);
  panic_if(tmp64 != parallelism, "FTL configuration mismatch.");

  RESTORE_SCALAR(in, tmp64);
  panic_if(tmp64 != totalSuperblock, "FTL configuration mismatch.");

  RESTORE_SCALAR(in, lastAllocated);
  RESTORE_BLOB(in, eraseCountList, sizeof(uint32_t) * totalSuperblock);
  RESTORE_BLOB(in, inUseBlockMap, sizeof(PPN) * parallelism);
  RESTORE_SCALAR(in, freeBlockCount);

  uint64_t size;

  for (uint64_t i = 0; i < parallelism; i++) {
    RESTORE_SCALAR(in, size);

    for (uint64_t j = 0; j < size; j++) {
      PPN id;

      RESTORE_SCALAR(in, id);

      freeBlocks[i].emplace_back(id);
    }
  }

  for (uint64_t i = 0; i < parallelism; i++) {
    RESTORE_SCALAR(in, size);

    for (uint64_t j = 0; j < size; j++) {
      PPN id;

      RESTORE_SCALAR(in, id);

      fullBlocks[i].emplace_back(id);
    }
  }

  RESTORE_SCALAR(in, selectionMode);
  RESTORE_SCALAR(in, fgcThreshold);
  RESTORE_SCALAR(in, bgcThreshold);
  RESTORE_SCALAR(in, dchoice);
}

}  // namespace SimpleSSD::FTL::BlockAllocator
