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
    OverProvisioningRatio,
    EraseThreshold,

    // Filling
    FillingMode,
    FillRatio,
    InvalidFillRatio,

    // Garbage Collection
    VictimSelectionPolicy,
    DChoiceParam,
    GCThreshold,
    GCMode,
    GCReclaimBlocks,
    GCReclaimThreshold,

    // Common FTL setting
    UseSuperpage,
    SuperpageAllocation,

    // VLFTL
    VLTableRatio,
    MergeBeginThreshold,
    MergeEndThreshold,
  };

  enum class MappingType : uint8_t {
    PageLevelFTL,
    BlockLevelFTL,
    VLFTL,
  };

  enum class FillingType : uint8_t {
    SequentialSequential,
    SequentialRandom,
    RandomRandom,
  };

  enum class VictimSelectionMode : uint8_t {
    Greedy,
    Random,
    DChoice,
  };

  enum class GCBlockReclaimMode : uint8_t {
    ByCount,
    ByRatio,
  };

 private:
  MappingType mappingMode;
  float overProvision;
  uint64_t eraseThreshold;
  FillingType fillingMode;
  float fillRatio;
  float invalidFillRatio;

  VictimSelectionMode gcBlockSelection;
  uint64_t dChoiceParam;
  float gcThreshold;
  GCBlockReclaimMode gcMode;
  uint64_t gcReclaimBlocks;
  float gcReclaimThreshold;
  bool useSuperpage;
  uint8_t superpageAllocation;
  float pmTableRatio;
  float mergeBeginThreshold;
  float mergeEndThreshold;

  std::string superpage;

 public:
  Config();
  ~Config();

  const char *getSectionName() override { return "ftl"; }

  void loadFrom(pugi::xml_node &) override;
  void storeTo(pugi::xml_node &) override;
  void update() override;

  uint64_t readUint(uint32_t) override;
  float readFloat(uint32_t) override;
  bool readBoolean(uint32_t) override;
  bool writeUint(uint32_t, uint64_t) override;
  bool writeFloat(uint32_t, float) override;
  bool writeBoolean(uint32_t, bool) override;
};

}  // namespace SimpleSSD::FTL

#endif
