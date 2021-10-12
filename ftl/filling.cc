// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "ftl/filling.hh"

#include <random>

#include "ftl/base/abstract_ftl.hh"

namespace SimpleSSD::FTL {

Filling::Filling(ObjectData &o, FTLObjectData &fo) : Object(o), ftlobject(fo) {}

void Filling::start() noexcept {
  std::default_random_engine gen(std::random_device{}());

  uint64_t totalLogicalSuperPages;
  uint64_t totalPhysicalSuperBlocks;
  uint64_t nPagesToWarmup;
  uint64_t nPagesToInvalidate;
  uint64_t maxPagesBeforeGC;
  uint64_t valid;
  uint64_t invalid;
  Config::FillingType mode;
  PSPN pspn;
  std::vector<uint8_t> spare;

  auto filparam = object.config->getNANDStructure();
  auto param = ftlobject.pMapping->getInfo();

  debugprint(Log::DebugID::FTL, "Initialization started");

  totalLogicalSuperPages = param->totalLogicalPages / param->superpage;
  totalPhysicalSuperBlocks = param->totalPhysicalBlocks / param->superpage;

  float threshold = readConfigFloat(Section::FlashTranslation,
                                    Config::Key::BackgroundGCThreshold);

  nPagesToWarmup = (uint64_t)(
      totalLogicalSuperPages *
      readConfigFloat(Section::FlashTranslation, Config::Key::FillRatio));
  nPagesToInvalidate = (uint64_t)(
      totalLogicalSuperPages * readConfigFloat(Section::FlashTranslation,
                                               Config::Key::InvalidFillRatio));
  mode = (Config::FillingType)readConfigUint(Section::FlashTranslation,
                                             Config::Key::FillingMode);
  maxPagesBeforeGC = (uint64_t)(totalPhysicalSuperBlocks * threshold);
  maxPagesBeforeGC = totalPhysicalSuperBlocks - maxPagesBeforeGC;
  maxPagesBeforeGC *= filparam->page;

  if (nPagesToWarmup + nPagesToInvalidate > maxPagesBeforeGC) {
    warn("ftl: Too high filling ratio. Adjusting invalidPageRatio.");
    nPagesToInvalidate = maxPagesBeforeGC - nPagesToWarmup;
  }

  debugprint(Log::DebugID::FTL, "Total logical pages: %" PRIu64,
             totalLogicalSuperPages);
  debugprint(Log::DebugID::FTL,
             "Total logical pages to fill: %" PRIu64 " (%.2f %%)",
             nPagesToWarmup, nPagesToWarmup * 100.f / totalLogicalSuperPages);
  debugprint(Log::DebugID::FTL,
             "Total invalidated pages to create: %" PRIu64 " (%.2f %%)",
             nPagesToInvalidate,
             nPagesToInvalidate * 100.f / totalLogicalSuperPages);

  // Step 1. Filling
  if (mode == Config::FillingType::SequentialSequential ||
      mode == Config::FillingType::SequentialRandom) {
    // Sequential
    for (uint64_t i = 0; i < nPagesToWarmup; i++) {
      ftlobject.pMapping->writeMapping(static_cast<LSPN>(i), pspn);

      for (uint32_t j = 0; j < param->superpage; j++) {
        auto _ppn = param->makePPN(pspn, j);
        auto _lpn = param->makeLPN(static_cast<LSPN>(i), j);

        ftlobject.pFTL->writeSpare(_ppn, (uint8_t *)&_lpn, sizeof(LPN));
      }
    }
  }
  else {
    // Random
    std::uniform_int_distribution<uint64_t> dist(0, totalLogicalSuperPages - 1);

    for (uint64_t i = 0; i < nPagesToWarmup; i++) {
      LSPN lspn = static_cast<LSPN>(dist(gen));

      ftlobject.pMapping->writeMapping(lspn, pspn);

      for (uint32_t j = 0; j < param->superpage; j++) {
        auto _ppn = param->makePPN(pspn, j);
        auto _lpn = param->makeLPN(lspn, j);

        ftlobject.pFTL->writeSpare(_ppn, (uint8_t *)&_lpn, sizeof(LPN));
      }
    }
  }

  // Step 2. Invalidating
  if (mode == Config::FillingType::SequentialSequential) {
    // Sequential
    for (uint64_t i = 0; i < nPagesToInvalidate; i++) {
      ftlobject.pMapping->writeMapping(static_cast<LSPN>(i), pspn);

      for (uint32_t j = 0; j < param->superpage; j++) {
        auto _ppn = param->makePPN(pspn, j);
        auto _lpn = param->makeLPN(static_cast<LSPN>(i), j);

        ftlobject.pFTL->writeSpare(_ppn, (uint8_t *)&_lpn, sizeof(LPN));
      }
    }
  }
  else if (mode == Config::FillingType::SequentialRandom) {
    // Random
    // We can successfully restrict range of LPN to create exact number of
    // invalid pages because we wrote in sequential mannor in step 1.
    std::uniform_int_distribution<uint64_t> dist(0, nPagesToWarmup - 1);

    for (uint64_t i = 0; i < nPagesToInvalidate; i++) {
      LSPN lspn = static_cast<LSPN>(dist(gen));

      ftlobject.pMapping->writeMapping(lspn, pspn);

      for (uint32_t j = 0; j < param->superpage; j++) {
        auto _ppn = param->makePPN(pspn, j);
        auto _lpn = param->makeLPN(lspn, j);

        ftlobject.pFTL->writeSpare(_ppn, (uint8_t *)&_lpn, sizeof(LPN));
      }
    }
  }
  else {
    // Random
    std::uniform_int_distribution<uint64_t> dist(0, totalLogicalSuperPages - 1);

    for (uint64_t i = 0; i < nPagesToInvalidate; i++) {
      LSPN lspn = static_cast<LSPN>(dist(gen));

      ftlobject.pMapping->writeMapping(lspn, pspn);

      for (uint32_t j = 0; j < param->superpage; j++) {
        auto _ppn = param->makePPN(pspn, j);
        auto _lpn = param->makeLPN(lspn, j);

        ftlobject.pFTL->writeSpare(_ppn, (uint8_t *)&_lpn, sizeof(LPN));
      }
    }
  }

  // Report
  ftlobject.pAllocator->getPageStatistics(valid, invalid);

  debugprint(Log::DebugID::FTL, "Filling finished. Page status:");
  debugprint(Log::DebugID::FTL,
             "  Total valid physical pages: %" PRIu64
             " (%.2f %%, target: %" PRIu64 ", error: %" PRId64 ")",
             valid, valid * 100.f / totalLogicalSuperPages, nPagesToWarmup,
             (int64_t)(valid - nPagesToWarmup));
  debugprint(Log::DebugID::FTL,
             "  Total invalid physical pages: %" PRIu64
             " (%.2f %%, target: %" PRIu64 ", error: %" PRId64 ")",
             invalid, invalid * 100.f / totalLogicalSuperPages,
             nPagesToInvalidate, (int64_t)(invalid - nPagesToInvalidate));

  // Filling P/E Cycles
  uint32_t targetCycle =
      readConfigUint(Section::FlashTranslation, Config::Key::EraseCount);

  if (UNLIKELY(targetCycle > 0)) {
    for (uint64_t psbn = 0; psbn < totalPhysicalSuperBlocks; psbn++) {
      auto &bmeta = ftlobject.pAllocator->getBlockMetadata(PSBN{psbn});

      bmeta.erasedCount = targetCycle;
    }
  }

  debugprint(Log::DebugID::FTL, "Initialization finished");
}

void Filling::getStatList(std::vector<Stat> &, std::string) noexcept {}

void Filling::getStatValues(std::vector<double> &) noexcept {}

void Filling::resetStatValues() noexcept {}

void Filling::createCheckpoint(std::ostream &) const noexcept {}

void Filling::restoreCheckpoint(std::istream &) noexcept {}

}  // namespace SimpleSSD::FTL
