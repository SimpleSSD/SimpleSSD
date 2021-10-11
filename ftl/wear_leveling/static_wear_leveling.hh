// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_FTL_WEAR_LEVELING_STATIC_WEAR_LEVELING_HH__
#define __SIMPLESSD_FTL_WEAR_LEVELING_STATIC_WEAR_LEVELING_HH__

#include "ftl/wear_leveling/abstract_wear_leveling.hh"

namespace SimpleSSD::FTL::WearLeveling {

class StaticWearLeveling : public AbstractWearLeveling {
 protected:
  uint64_t beginAt;

  BlockAllocator::AbstractVictimSelection *method;

  double threshold;

  struct {
    uint64_t foreground;
    uint64_t background;
    uint64_t copiedPages;
    uint64_t erasedBlocks;
  } stat;

  Log::DebugID getDebugLogID() override {
    return Log::DebugID::FTL_StaticWearLeveling;
  }

  virtual void triggerForeground(uint64_t now);

  void blockEraseCallback(uint64_t now, const PSBN &erased) override;

  void readPage(uint64_t, uint32_t) override;
  void updateMapping(uint64_t, uint32_t) override;
  void done(uint64_t, uint32_t) override;

 public:
  StaticWearLeveling(ObjectData &, FTLObjectData &, FIL::FIL *);
  ~StaticWearLeveling();

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::FTL::WearLeveling

#endif
