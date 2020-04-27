// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "ftl/allocator/basic_allocator.hh"

#include "ftl/mapping/abstract_mapping.hh"

namespace SimpleSSD::FTL::BlockAllocator {

BasicAllocator::BasicAllocator(ObjectData &o, Mapping::AbstractMapping *m)
    : AbstractAllocator(o, m),
      inUseBlockMap(nullptr),
      freeBlocks(nullptr),
      mtengine(rd()) {
  selectionMode = (Config::VictimSelectionMode)readConfigUint(
      Section::FlashTranslation, Config::Key::VictimSelectionPolicy);
  dchoice =
      readConfigUint(Section::FlashTranslation, Config::Key::DChoiceParam);
  gcThreshold =
      readConfigFloat(Section::FlashTranslation, Config::Key::GCThreshold);

  switch (selectionMode) {
    case Config::VictimSelectionMode::Random:
      victimSelectionFunction = [this](uint64_t idx, std::deque<PPN> &list) {
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
      };

      break;
    case Config::VictimSelectionMode::Greedy:
      victimSelectionFunction = [this](uint64_t idx, std::deque<PPN> &list) {
        CPU::Function fstat;
        CPU::markFunction(fstat);

        auto &currentList = fullBlocks[idx];
        std::vector<std::pair<std::list<PPN>::iterator, uint32_t>> valid;

        // Collect valid pages
        for (auto iter = currentList.begin(); iter != currentList.end();
             ++iter) {
          valid.emplace_back(iter, pMapper->getValidPages(*iter, superpage));
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
      };

      break;
    case Config::VictimSelectionMode::CostBenefit:
      victimSelectionFunction =
          [this, pageCount = object.config->getNANDStructure()->page](
              uint64_t idx, std::deque<PPN> &list) {
            CPU::Function fstat;
            CPU::markFunction(fstat);

            auto &currentList = fullBlocks[idx];
            std::vector<std::pair<std::list<PPN>::iterator, float>> valid;

            // Collect valid pages
            for (auto iter = currentList.begin(); iter != currentList.end();
                 ++iter) {
              float util = pMapper->getValidPages(*iter, superpage) / pageCount;

              util = util / ((1.f - util) * pMapper->getAge(*iter, superpage));

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
          };

      break;
    case Config::VictimSelectionMode::DChoice:
      victimSelectionFunction = [this](uint64_t idx, std::deque<PPN> &list) {
        CPU::Function fstat;
        CPU::markFunction(fstat);

        auto &currentList = fullBlocks[idx];
        std::uniform_int_distribution<uint64_t> dist(0, currentList.size() - 1);
        std::vector<uint64_t> offsets;
        std::vector<std::pair<std::list<PPN>::iterator, uint32_t>> valid;

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
            valid.emplace_back(iter, pMapper->getValidPages(*iter, superpage));
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
      };

      break;
    default:
      panic("Unexpected victim block selection mode.");

      break;
  }
}

BasicAllocator::~BasicAllocator() {
  free(eraseCountList);
  free(inUseBlockMap);
  delete[] freeBlocks;
  delete[] fullBlocks;
}

void BasicAllocator::initialize(Parameter *p) {
  AbstractAllocator::initialize(p);

  superpage = param->superpage;
  parallelism = param->parallelism / superpage;
  totalSuperblock = param->totalPhysicalBlocks / superpage;
  freeBlockCount = totalSuperblock;
  fullBlockCount = 0;

  if ((float)parallelism / totalSuperblock * 2.f >= gcThreshold) {
    warn("GC threshold cannot hold minimum blocks. Adjust threshold.");

    gcThreshold = (float)(parallelism + 1) / totalSuperblock * 2.f;
  }

  // Allocate data
  eraseCountList = (uint32_t *)calloc(totalSuperblock, sizeof(uint32_t));
  inUseBlockMap = (PPN *)calloc(parallelism, sizeof(PPN));
  freeBlocks = new std::list<PPN>[parallelism]();
  fullBlocks = new std::list<PPN>[parallelism]();

  lastAllocated = 0;

  // Fill data
  uint64_t left = totalSuperblock / parallelism;

  for (uint64_t i = 0; i < parallelism; i++) {
    for (uint64_t j = 0; j < left; j++) {
      freeBlocks[i].emplace_back(i + j * parallelism);
    }
  }
}

CPU::Function BasicAllocator::allocateBlock(PPN &blockUsed, uint64_t np) {
  CPU::Function fstat;
  CPU::markFunction(fstat);

  panic_if(np != superpage, "Invalid access from mapping.");

  PPN idx = lastAllocated;

  if (LIKELY(blockUsed != InvalidPPN)) {
    blockUsed /= np;

    idx = getParallelismFromSPPN(blockUsed);

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
  blockUsed = inUseBlockMap[idx] * np;

  freeBlocks[idx].pop_front();
  freeBlockCount--;

  return fstat;
}

PPN BasicAllocator::getBlockAt(PPN idx, uint64_t np) {
  panic_if(np != superpage, "Invalid access from mapping.");

  if (idx == InvalidPPN) {
    PPN ppn = inUseBlockMap[lastAllocated++];

    if (lastAllocated == parallelism) {
      lastAllocated = 0;
    }

    return ppn * np;
  }

  idx /= np;

  panic_if(idx >= parallelism, "Invalid parallelism index.");

  return inUseBlockMap[idx] * np;
}

bool BasicAllocator::checkGCThreshold() {
  return (float)freeBlockCount / totalSuperblock < gcThreshold;
}

void BasicAllocator::getVictimBlocks(std::deque<PPN> &list, Event eid) {
  CPU::Function fstat;
  CPU::markFunction(fstat);

  list.clear();

  if (UNLIKELY(fullBlockCount <= parallelism * dchoice)) {
    // Just return least erased blocks
    for (uint64_t i = 0; i < parallelism; i++) {
      list.emplace_back(fullBlocks[i].front());

      fullBlocks[i].pop_front();
      fullBlockCount--;
    }
  }
  else {
    // Select victim blocks
    for (uint64_t i = 0; i < parallelism; i++) {
      fstat += victimSelectionFunction(i, list);
    }
  }

  scheduleFunction(CPU::CPUGroup::FlashTranslationLayer, eid, fstat);
}

void BasicAllocator::reclaimBlocks(PPN blockID, Event eid) {
  CPU::Function fstat;
  CPU::markFunction(fstat);

  panic_if(blockID >= totalSuperblock, "Invalid block ID.");

  // Insert PPN to free block list
  PPN idx = getParallelismFromSPPN(blockID);
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

void BasicAllocator::getStatList(std::vector<Stat> &list,
                                 std::string prefix) noexcept {
  list.emplace_back(prefix + "wear_leveling", "Wear-leveling factor");
}

void BasicAllocator::getStatValues(std::vector<double> &values) noexcept {
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

void BasicAllocator::resetStatValues() noexcept {}

void BasicAllocator::createCheckpoint(std::ostream &out) const noexcept {
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
  BACKUP_SCALAR(out, gcThreshold);
  BACKUP_SCALAR(out, dchoice);
}

void BasicAllocator::restoreCheckpoint(std::istream &in) noexcept {
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
  RESTORE_SCALAR(in, gcThreshold);
  RESTORE_SCALAR(in, dchoice);
}

}  // namespace SimpleSSD::FTL::BlockAllocator
