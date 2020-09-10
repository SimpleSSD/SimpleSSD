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

    // Filling
    FillingMode,
    FillRatio,
    InvalidFillRatio,

    // Garbage Collection
    VictimSelectionPolicy,
    DChoiceParam,
    FGCThreshold,
    BGCThreshold,
    BGCIdleTime,

    // Common FTL setting
    OverProvisioningRatio,
    SuperpageAllocation,
    MergeReadModifyWrite,
    DemandPaging,
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

  enum class VictimSelectionMode : uint8_t {
    Greedy,
    Random,
    CostBenefit,
    DChoice,
  };

 private:
  float overProvision;
  float fillRatio;
  float invalidFillRatio;

  MappingType mappingMode;
  FillingType fillingMode;
  bool mergeRMW;
  bool demandPaging;

  uint64_t dChoiceParam;
  uint64_t bgcIdletime;
  float fgcThreshold;
  float bgcThreshold;
  VictimSelectionMode gcBlockSelection;
  uint8_t superpageAllocation;

  std::string superpage;

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
