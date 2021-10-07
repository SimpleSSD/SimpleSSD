// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_FTL_CONFIG_HH__
#define __SIMPLESSD_FTL_CONFIG_HH__

#include "fil/def.hh"
#include "sim/base_config.hh"

namespace SimpleSSD::FTL {

class Config : public BaseConfig {
 public:
  enum Key : uint32_t {
    MappingMode,

    // Common FTL setting
    OverProvisioningRatio,
    SuperpageAllocation,
    MergeReadModifyWrite,

    // Filling
    FillingMode,
    FillRatio,
    InvalidFillRatio,
    EraseCount,

    // Background jobs
    // Garbage Collection
    GCMode,
    ForegroundGCThreshold,
    BackgroundGCThreshold,
    IdleTimeForBackgroundGC,
    VictimSelectionPolicy,
    SamplingFactor,
    ForegroundBlockEraseLevel,
    BackgroundBlockEraseLevel,

    // Wear-leveling
    WearLevelingMode,
    StaticWearLevelingThreshold,

    // Read reclaim
    ReadReclaimMode,
  };

  enum class MappingType : uint8_t {
    PageLevelFTL,
    BlockLevelFTL,
  };

  enum class FillingType : uint8_t {
    SequentialSequential,
    SequentialRandom,
    RandomRandom,
  };

  enum class GCType : uint8_t {
    Naive,
    Advanced,
    Preemptible,
  };

  enum class WearLevelingType : uint8_t {
    None,
    Static,
  };

  enum class ReadReclaimType : uint8_t {
    None,
    Basic,
  };

  enum class VictimSelectionMode : uint8_t {
    Greedy,
    Random,
    CostBenefit,
    DChoice,
  };

  enum class Granularity : uint8_t {
    None,
    FirstLevel,
    SecondLevel,
    ThirdLevel,
    AllLevel,
  };

 private:
  float overProvision;
  float fillRatio;
  float invalidFillRatio;

  MappingType mappingMode;
  FillingType fillingMode;
  GCType gcMode;
  VictimSelectionMode gcBlockSelection;

  float fgcThreshold;
  float bgcThreshold;
  uint64_t bgcIdletime;
  uint64_t dChoiceParam;

  uint8_t superpageAllocation;
  bool mergeRMW;
  Granularity fgcBlockEraseLevel;
  Granularity bgcBlockEraseLevel;
  WearLevelingType wlMode;
  ReadReclaimType rrMode;

  float staticWearLevelingThreshold;

  std::string superpage;

  void loadGC(pugi::xml_node &) noexcept;
  void loadWearLeveling(pugi::xml_node &) noexcept;
  void loadReadReclaim(pugi::xml_node &) noexcept;
  void storeGC(pugi::xml_node &) noexcept;
  void storeWearLeveling(pugi::xml_node &) noexcept;
  void storeReadReclaim(pugi::xml_node &) noexcept;

 public:
  Config();

  const char *getSectionName() noexcept override { return "ftl"; }

  void loadFrom(pugi::xml_node &) noexcept override;
  void storeTo(pugi::xml_node &) noexcept override;
  void update() noexcept override;

  uint64_t readUint(uint32_t) const noexcept override;
  float readFloat(uint32_t) const noexcept override;
  bool readBoolean(uint32_t) const noexcept override;
  bool writeUint(uint32_t, uint64_t) noexcept override;
  bool writeFloat(uint32_t, float) noexcept override;
  bool writeBoolean(uint32_t, bool) noexcept override;
};

}  // namespace SimpleSSD::FTL

#endif
