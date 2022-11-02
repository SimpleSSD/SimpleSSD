// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "ftl/allocator/victim_selection.hh"

#include <random>

#include "ftl/allocator/abstract_allocator.hh"

namespace SimpleSSD::FTL::BlockAllocator {

AbstractVictimSelection::AbstractVictimSelection(AbstractAllocator *p)
    : pAllocator(p) {}

AbstractVictimSelection::~AbstractVictimSelection() {}

class RandomVictimSelection : public AbstractVictimSelection {
 protected:
  std::default_random_engine engine;

 public:
  RandomVictimSelection(AbstractAllocator *p)
      : AbstractVictimSelection(p), engine(std::random_device{}()) {}

  CPU::Function getVictim(uint32_t idx,
                          std::list<PSBN>::iterator &iter) noexcept override {
    CPU::Function fstat;
    CPU::markFunction(fstat);

    auto &currentList = pAllocator->getBlockListAtParallelismIndex(idx);
    std::uniform_int_distribution<uint64_t> dist(0, currentList.size() - 1);

    // Select one block from current full block list
    uint64_t ridx = dist(engine);

    // Get block ID from list
    iter = currentList.begin();

    // O(n)
    std::advance(iter, ridx);

    return fstat;
  }
};

class GreedyVictimSelection : public AbstractVictimSelection {
 public:
  GreedyVictimSelection(AbstractAllocator *p) : AbstractVictimSelection(p) {}

  CPU::Function getVictim(
      uint32_t idx, std::list<PSBN>::iterator &minIndex) noexcept override {
    CPU::Function fstat;
    CPU::markFunction(fstat);

    auto &currentList = pAllocator->getBlockListAtParallelismIndex(idx);
    uint32_t min = std::numeric_limits<uint32_t>::max();

    minIndex = currentList.begin();

    // Collect valid pages and find min value
    for (auto iter = currentList.begin(); iter != currentList.end(); ++iter) {
      auto valid = pAllocator->getBlockMetadata(*iter).validPages.count();

      if (min > valid) {
        min = valid;
        minIndex = iter;
      }
    }

    return fstat;
  }
};

class CostBenefitVictimSelection : public AbstractVictimSelection {
 protected:
  const uint32_t pageCount;

 public:
  CostBenefitVictimSelection(ObjectData &o, AbstractAllocator *p)
      : AbstractVictimSelection(p),
        pageCount(o.config->getNANDStructure()->page) {}

  CPU::Function getVictim(
      uint32_t idx, std::list<PSBN>::iterator &minIndex) noexcept override {
    CPU::Function fstat;
    CPU::markFunction(fstat);

    auto &currentList = pAllocator->getBlockListAtParallelismIndex(idx);
    float min = std::numeric_limits<float>::max();
    minIndex = currentList.begin();

    // Collect valid pages and find min value
    for (auto iter = currentList.begin(); iter != currentList.end(); ++iter) {
      float util =
          (float)pAllocator->getBlockMetadata(*iter).validPages.count() /
          pageCount;

      util = util /
             ((1.f - util) * pAllocator->getBlockMetadata(*iter).insertedAt);

      if (min > util) {
        min = util;
        minIndex = iter;
      }
    }

    return fstat;
  }
};

class DChoiceVictimSelection : public AbstractVictimSelection {
 protected:
  const uint64_t dchoice;
  std::default_random_engine engine;

 public:
  DChoiceVictimSelection(ObjectData &o, AbstractAllocator *p)
      : AbstractVictimSelection(p),
        dchoice(o.config->readUint(Section::FlashTranslation,
                                   Config::Key::SamplingFactor)),
        engine(std::random_device{}()) {}

  CPU::Function getVictim(
      uint32_t idx, std::list<PSBN>::iterator &minIndex) noexcept override {
    CPU::Function fstat;
    CPU::markFunction(fstat);

    auto &currentList = pAllocator->getBlockListAtParallelismIndex(idx);
    const auto listLength = currentList.size();

    if (UNLIKELY(listLength <= dchoice)) {
      // Just return least erased blocks
      minIndex = currentList.begin();

      return fstat;
    }

    std::uniform_int_distribution<uint64_t> dist(0, listLength - 1);
    std::unordered_set<uint64_t> offsets;

    // Select dchoice number of blocks from current full block list
    offsets.reserve(dchoice);

    for (auto i = listLength - dchoice; i < listLength; i++) {
      auto idx = dist(engine);
      auto iter = offsets.emplace(idx);

      if (!iter.second) {
        offsets.emplace(i);
      }
    }

    // Greedy
    auto iter = currentList.begin();
    uint32_t min = std::numeric_limits<uint32_t>::max();

    for (uint64_t i = 0; i < listLength; i++) {
      if (offsets.find(i) != offsets.end()) {
        auto valid = pAllocator->getBlockMetadata(*iter).validPages.count();

        if (min > valid) {
          min = valid;
          minIndex = iter;
        }
      }

      ++iter;
    }

    return fstat;
  }
};

class LeastErasedVictimSelection : public AbstractVictimSelection {
 public:
  LeastErasedVictimSelection(AbstractAllocator *p)
      : AbstractVictimSelection(p) {}

  CPU::Function getVictim(
      uint32_t idx, std::list<PSBN>::iterator &minIndex) noexcept override {
    CPU::Function fstat;
    CPU::markFunction(fstat);

    minIndex = pAllocator->getBlockListAtParallelismIndex(idx).begin();

    return fstat;
  }
};

class LeastReadVictimSelection : public AbstractVictimSelection {
 public:
  LeastReadVictimSelection(AbstractAllocator *p) : AbstractVictimSelection(p) {}

  CPU::Function getVictim(
      uint32_t idx, std::list<PSBN>::iterator &minIndex) noexcept override {
    CPU::Function fstat;
    CPU::markFunction(fstat);

    auto &currentList = pAllocator->getBlockListAtParallelismIndex(idx);

    uint32_t min = std::numeric_limits<uint32_t>::max();

    minIndex = currentList.begin();

    // Collect valid pages and find min value
    for (auto iter = currentList.begin(); iter != currentList.end(); ++iter) {
      auto read = pAllocator->getBlockMetadata(*iter).readCountAfterErase;

      if (min > read) {
        min = read;
        minIndex = iter;
      }
    }

    return fstat;
  }
};

class MostErasedVictimSelection : public AbstractVictimSelection {
 public:
  MostErasedVictimSelection(AbstractAllocator *p)
      : AbstractVictimSelection(p) {}

  CPU::Function getVictim(
      uint32_t idx, std::list<PSBN>::iterator &maxIndex) noexcept override {
    CPU::Function fstat;
    CPU::markFunction(fstat);

    maxIndex = pAllocator->getBlockListAtParallelismIndex(idx).end();
    --maxIndex;

    return fstat;
  }
};

class MostReadVictimSelection : public AbstractVictimSelection {
 public:
  MostReadVictimSelection(AbstractAllocator *p) : AbstractVictimSelection(p) {}

  CPU::Function getVictim(
      uint32_t idx, std::list<PSBN>::iterator &maxIndex) noexcept override {
    CPU::Function fstat;
    CPU::markFunction(fstat);

    auto &currentList = pAllocator->getBlockListAtParallelismIndex(idx);

    uint32_t max = 0;

    maxIndex = currentList.begin();

    // Collect valid pages and find min value
    for (auto iter = currentList.begin(); iter != currentList.end(); ++iter) {
      auto read = pAllocator->getBlockMetadata(*iter).readCountAfterErase;

      if (max < read) {
        max = read;
        maxIndex = iter;
      }
    }

    return fstat;
  }
};

class LRUVictimSelection : public AbstractVictimSelection {
 public:
  LRUVictimSelection(AbstractAllocator *p) : AbstractVictimSelection(p) {}

  CPU::Function getVictim(
      uint32_t idx, std::list<PSBN>::iterator &minIndex) noexcept override {
    CPU::Function fstat;
    CPU::markFunction(fstat);

    auto &currentList = pAllocator->getBlockListAtParallelismIndex(idx);

    uint64_t min = std::numeric_limits<uint64_t>::max();

    minIndex = currentList.begin();

    // Collect valid pages and find min value
    for (auto iter = currentList.begin(); iter != currentList.end(); ++iter) {
      auto tick = pAllocator->getBlockMetadata(*iter).insertedAt;

      if (min > tick) {
        min = tick;
        minIndex = iter;
      }
    }

    return fstat;
  }
};

class MRUVictimSelection : public AbstractVictimSelection {
 public:
  MRUVictimSelection(AbstractAllocator *p) : AbstractVictimSelection(p) {}

  CPU::Function getVictim(
      uint32_t idx, std::list<PSBN>::iterator &maxIndex) noexcept override {
    CPU::Function fstat;
    CPU::markFunction(fstat);

    auto &currentList = pAllocator->getBlockListAtParallelismIndex(idx);

    uint64_t max = std::numeric_limits<uint64_t>::max();

    maxIndex = currentList.begin();

    // Collect valid pages and find min value
    for (auto iter = currentList.begin(); iter != currentList.end(); ++iter) {
      auto tick = pAllocator->getBlockMetadata(*iter).insertedAt;

      if (max < tick) {
        max = tick;
        maxIndex = iter;
      }
    }

    return fstat;
  }
};

AbstractVictimSelection *
VictimSelectionFactory::createVictiomSelectionAlgorithm(
    ObjectData &object, AbstractAllocator *pAllocator, VictimSelectionID id) {
  switch (id) {
    case VictimSelectionID::Random:
      return new RandomVictimSelection(pAllocator);
    case VictimSelectionID::Greedy:
      return new GreedyVictimSelection(pAllocator);
    case VictimSelectionID::CostBenefit:
      return new CostBenefitVictimSelection(object, pAllocator);
    case VictimSelectionID::DChoice:
      return new DChoiceVictimSelection(object, pAllocator);
    case VictimSelectionID::LeastErased:
      return new LeastErasedVictimSelection(pAllocator);
    case VictimSelectionID::LeastRead:
      return new LeastReadVictimSelection(pAllocator);
    case VictimSelectionID::MostErased:
      return new MostErasedVictimSelection(pAllocator);
    case VictimSelectionID::MostRead:
      return new MostReadVictimSelection(pAllocator);
    case VictimSelectionID::LeastRecentlyUsed:
      return new LRUVictimSelection(pAllocator);
    case VictimSelectionID::MostRecentlyUsed:
      return new MRUVictimSelection(pAllocator);
  }

  object.log->print(Log::LogID::Panic,
                    "Undefined VictimSelectionID specified.");

  return nullptr;
}

}  // namespace SimpleSSD::FTL::BlockAllocator
