// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "ftl/config.hh"

namespace SimpleSSD::FTL {

const char NAME_MAPPING_MODE[] = "MappingMode";

// common section
const char NAME_OVERPROVISION_RATIO[] = "OverProvisioningRatio";
const char NAME_SUPERPAGE_ALLOCATION[] = "SuperpageAllocation";
const char NAME_MERGE_RMW[] = "MergeReadModifyWrite";

// warmup section
const char NAME_FILLING_MODE[] = "FillingMode";
const char NAME_FILL_RATIO[] = "FillRatio";
const char NAME_INVALID_PAGE_RATIO[] = "InvalidFillRatio";
const char NAME_FILL_ERASE_COUNT[] = "EraseCount";

// background section
const char NAME_MODE[] = "Mode";

// background > gc > trigger section
const char NAME_FGC_THRESHOLD[] = "ForegroundThreshold";
const char NAME_BGC_THRESHOLD[] = "BackgroundThreshold";
const char NAME_BGC_IDLETIME[] = "IdletimeThreshold";

// background > gc > blockselection section
const char NAME_GC_EVICT_POLICY[] = "VictimSelectionPolicy";
const char NAME_GC_D_CHOICE_PARAM[] = "SamplingFactor";

// background > gc > blockerase strategy
const char NAME_FGC_PARALLEL_BLOCK_ERASE[] = "ForegroundBlockEraseLevel";
const char NAME_BGC_PARALLEL_BLOCK_ERASE[] = "BackgroundBlockEraseLevel";

// background > wearleveling
const char NAME_WEAR_LEVELING_THRESHOLD[] = "StaticThreshold";

Config::Config() {
  mappingMode = MappingType::PageLevelFTL;
  overProvision = 0.2f;
  fillingMode = FillingType::SequentialSequential;
  fillRatio = 1.f;
  invalidFillRatio = 0.f;
  fillingEraseCount = 0;

  gcMode = GCType::Naive;

  gcBlockSelection = VictimSelectionMode::Greedy;
  dChoiceParam = 3;
  fgcThreshold = 0.05f;
  bgcThreshold = 0.1f;
  bgcIdletime = 5000000000000ul;

  fgcBlockEraseLevel = Granularity::ThirdLevel;
  bgcBlockEraseLevel = Granularity::None;

  wlMode = WearLevelingType::None;
  staticWearLevelingThreshold = 0.7f;

  rrMode = ReadReclaimType::None;

  mergeRMW = false;

  superpageAllocation = FIL::PageAllocation::None;
}

void Config::loadGC(pugi::xml_node &section) noexcept {
  for (auto node = section.first_child(); node; node = node.next_sibling()) {
    auto name = node.attribute("name").value();

    LOAD_NAME_UINT_TYPE(node, NAME_MODE, GCType, gcMode);

    if (strcmp(name, "trigger") == 0 && isSection(node)) {
      for (auto node2 = node.first_child(); node2;
           node2 = node2.next_sibling()) {
        LOAD_NAME_FLOAT(node2, NAME_FGC_THRESHOLD, fgcThreshold);
        LOAD_NAME_FLOAT(node2, NAME_BGC_THRESHOLD, bgcThreshold);
        LOAD_NAME_TIME(node2, NAME_BGC_IDLETIME, bgcIdletime);
      }
    }
    else if (strcmp(name, "blockselection") == 0 && isSection(node)) {
      for (auto node2 = node.first_child(); node2;
           node2 = node2.next_sibling()) {
        LOAD_NAME_UINT_TYPE(node2, NAME_GC_EVICT_POLICY, VictimSelectionMode,
                            gcBlockSelection);
        LOAD_NAME_UINT(node2, NAME_GC_D_CHOICE_PARAM, dChoiceParam);
      }
    }
    else if (strcmp(name, "blockerase") == 0 && isSection(node)) {
      for (auto node2 = node.first_child(); node2;
           node2 = node2.next_sibling()) {
        LOAD_NAME_UINT_TYPE(node2, NAME_FGC_PARALLEL_BLOCK_ERASE, Granularity,
                            fgcBlockEraseLevel);
        LOAD_NAME_UINT_TYPE(node2, NAME_BGC_PARALLEL_BLOCK_ERASE, Granularity,
                            bgcBlockEraseLevel);
      }
    }
  }
}

void Config::loadWearLeveling(pugi::xml_node &section) noexcept {
  for (auto node = section.first_child(); node; node = node.next_sibling()) {
    LOAD_NAME_UINT_TYPE(node, NAME_MODE, WearLevelingType, wlMode);
    LOAD_NAME_FLOAT(node, NAME_WEAR_LEVELING_THRESHOLD,
                    staticWearLevelingThreshold);
  }
}

void Config::loadReadReclaim(pugi::xml_node &section) noexcept {
  for (auto node = section.first_child(); node; node = node.next_sibling()) {
    LOAD_NAME_UINT_TYPE(node, NAME_MODE, ReadReclaimType, rrMode);
  }
}

void Config::loadFrom(pugi::xml_node &section) noexcept {
  for (auto node = section.first_child(); node; node = node.next_sibling()) {
    auto name = node.attribute("name").value();

    LOAD_NAME_UINT_TYPE(node, NAME_MAPPING_MODE, MappingType, mappingMode);

    if (strcmp(name, "background") == 0 && isSection(node)) {
      for (auto node2 = node.first_child(); node2;
           node2 = node2.next_sibling()) {
        auto name2 = node2.attribute("name").value();

        if (strcmp(name2, "gc") == 0 && isSection(node2)) {
          loadGC(node2);
        }
        else if (strcmp(name2, "wearleveling") == 0 && isSection(node2)) {
          loadWearLeveling(node2);
        }
        else if (strcmp(name2, "readreclaim") == 0 && isSection(node2)) {
          loadReadReclaim(node2);
        }
      }
    }
    else if (strcmp(name, "common") == 0 && isSection(node)) {
      for (auto node2 = node.first_child(); node2;
           node2 = node2.next_sibling()) {
        LOAD_NAME_FLOAT(node2, NAME_OVERPROVISION_RATIO, overProvision);
        LOAD_NAME_STRING(node2, NAME_SUPERPAGE_ALLOCATION, superpage);
        LOAD_NAME_BOOLEAN(node2, NAME_MERGE_RMW, mergeRMW);
      }
    }
    else if (strcmp(name, "warmup") == 0 && isSection(node)) {
      for (auto node2 = node.first_child(); node2;
           node2 = node2.next_sibling()) {
        LOAD_NAME_UINT_TYPE(node2, NAME_FILLING_MODE, FillingType, fillingMode);
        LOAD_NAME_FLOAT(node2, NAME_FILL_RATIO, fillRatio);
        LOAD_NAME_FLOAT(node2, NAME_INVALID_PAGE_RATIO, invalidFillRatio);
        LOAD_NAME_UINT_TYPE(node2, NAME_FILL_ERASE_COUNT, uint32_t,
                            fillingEraseCount);
      }
    }
  }
}

void Config::storeGC(pugi::xml_node &section) noexcept {
  pugi::xml_node node;

  STORE_NAME_UINT(section, NAME_MODE, gcMode);

  STORE_SECTION(section, "trigger", node);
  STORE_NAME_FLOAT(node, NAME_FGC_THRESHOLD, fgcThreshold);
  STORE_NAME_FLOAT(node, NAME_BGC_THRESHOLD, bgcThreshold);
  STORE_NAME_TIME(node, NAME_BGC_IDLETIME, bgcIdletime);

  STORE_SECTION(section, "blockselection", node);
  STORE_NAME_UINT(node, NAME_GC_EVICT_POLICY, gcBlockSelection);
  STORE_NAME_UINT(node, NAME_GC_D_CHOICE_PARAM, dChoiceParam);

  STORE_SECTION(section, "blockerase", node);
  STORE_NAME_UINT(node, NAME_FGC_PARALLEL_BLOCK_ERASE, fgcBlockEraseLevel);
  STORE_NAME_UINT(node, NAME_BGC_PARALLEL_BLOCK_ERASE, bgcBlockEraseLevel);
}

void Config::storeWearLeveling(pugi::xml_node &section) noexcept {
  STORE_NAME_UINT(section, NAME_MODE, wlMode);
  STORE_NAME_FLOAT(section, NAME_WEAR_LEVELING_THRESHOLD,
                   staticWearLevelingThreshold);
};

void Config::storeReadReclaim(pugi::xml_node &section) noexcept {
  STORE_NAME_UINT(section, NAME_MODE, rrMode);
};

void Config::storeTo(pugi::xml_node &section) noexcept {
  pugi::xml_node node, node2;

  // Re-generate superpage allocation string
  const char table[4] = {'C', 'W', 'D', 'P'};

  superpage.clear();
  superpage.reserve(4);

  for (int i = 0; i < 4; i++) {
    uint8_t mask = 1 << i;

    if (superpageAllocation & mask) {
      superpage.push_back(table[i]);
    }
  }

  STORE_NAME_UINT(section, NAME_MAPPING_MODE, mappingMode);

  STORE_SECTION(section, "common", node);
  STORE_NAME_FLOAT(node, NAME_OVERPROVISION_RATIO, overProvision);
  STORE_NAME_STRING(node, NAME_SUPERPAGE_ALLOCATION, superpage);
  STORE_NAME_BOOLEAN(node, NAME_MERGE_RMW, mergeRMW);

  STORE_SECTION(section, "background", node);

  STORE_SECTION(node, "gc", node2);
  storeGC(node2);

  STORE_SECTION(node, "wearleveling", node2);
  storeWearLeveling(node2);

  STORE_SECTION(node, "readreclaim", node2);
  storeReadReclaim(node2);

  STORE_SECTION(section, "warmup", node);
  STORE_NAME_UINT(node, NAME_FILLING_MODE, fillingMode);
  STORE_NAME_FLOAT(node, NAME_FILL_RATIO, fillRatio);
  STORE_NAME_FLOAT(node, NAME_INVALID_PAGE_RATIO, invalidFillRatio);
  STORE_NAME_UINT(node, NAME_FILL_ERASE_COUNT, fillingEraseCount);
}

void Config::update() noexcept {
  panic_if((uint8_t)mappingMode > 1, "Invalid MappingMode.");
  panic_if((uint8_t)fillingMode > 2, "Invalid FillingMode.");
  panic_if((uint8_t)gcMode > 2, "Invalid GCMode.");
  panic_if((uint8_t)gcBlockSelection > 3, "Invalid VictimSelectionPolicy.");
  panic_if((uint8_t)fgcBlockEraseLevel > 4,
           "Invalid ForegroundBlockEraseLevel.");
  panic_if((uint8_t)bgcBlockEraseLevel > 4,
           "Invalid BackgroundBlockEraseLevel.");

  panic_if((uint8_t)wlMode > 1, "Invalid WearLevelingMode.");
  panic_if(
      staticWearLevelingThreshold <= 0.f || staticWearLevelingThreshold >= 1.f,
      "Invalid StaticThreshold.");

  panic_if((uint8_t)rrMode > 1, "Invalid ReadReclaimMode.");

  panic_if(fillRatio < 0.f || fillRatio > 1.f, "Invalid FillingRatio.");
  panic_if(invalidFillRatio < 0.f || invalidFillRatio > 1.f,
           "Invalid InvalidPageRatio.");

  panic_if(
      bgcThreshold <= fgcThreshold,
      "Background GC threshold should be larger than Foreground GC threshold.");

  if (superpage.length() > 0) {
    superpageAllocation = 0;

    for (auto &iter : superpage) {
      if ((iter == 'C') | (iter == 'c')) {
        superpageAllocation |= FIL::PageAllocation::Channel;
      }
      else if ((iter == 'W') | (iter == 'w')) {
        superpageAllocation |= FIL::PageAllocation::Way;
      }
      else if ((iter == 'D') | (iter == 'd')) {
        superpageAllocation |= FIL::PageAllocation::Die;
      }
      else if ((iter == 'P') | (iter == 'p')) {
        superpageAllocation |= FIL::PageAllocation::Plane;
      }
    }
  }
}

uint64_t Config::readUint(uint32_t idx) const noexcept {
  uint64_t ret = 0;

  switch (idx) {
    case MappingMode:
      ret = (uint64_t)mappingMode;
      break;
    case FillingMode:
      ret = (uint64_t)fillingMode;
      break;
    case EraseCount:
      ret = fillingEraseCount;
      break;
    case GCMode:
      ret = (uint64_t)gcMode;
      break;
    case VictimSelectionPolicy:
      ret = (uint64_t)gcBlockSelection;
      break;
    case SamplingFactor:
      ret = dChoiceParam;
      break;
    case SuperpageAllocation:
      ret = superpageAllocation;
      break;
    case ForegroundBlockEraseLevel:
      ret = (uint64_t)fgcBlockEraseLevel;
      break;
    case BackgroundBlockEraseLevel:
      ret = (uint64_t)bgcBlockEraseLevel;
      break;
    case IdleTimeForBackgroundGC:
      ret = bgcIdletime;
      break;
    case WearLevelingMode:
      ret = (uint64_t)wlMode;
      break;
    case ReadReclaimMode:
      ret = (uint64_t)rrMode;
      break;
  }

  return ret;
}

float Config::readFloat(uint32_t idx) const noexcept {
  float ret = 0.f;

  switch (idx) {
    case OverProvisioningRatio:
      ret = overProvision;
      break;
    case FillRatio:
      ret = fillRatio;
      break;
    case InvalidFillRatio:
      ret = invalidFillRatio;
      break;
    case ForegroundGCThreshold:
      ret = fgcThreshold;
      break;
    case BackgroundGCThreshold:
      ret = bgcThreshold;
      break;
    case StaticWearLevelingThreshold:
      ret = staticWearLevelingThreshold;
      break;
  }

  return ret;
}

bool Config::readBoolean(uint32_t idx) const noexcept {
  switch (idx) {
    case MergeReadModifyWrite:
      return mergeRMW;
      break;
  }

  return false;
}

bool Config::writeUint(uint32_t idx, uint64_t value) noexcept {
  bool ret = true;

  switch (idx) {
    case MappingMode:
      mappingMode = (MappingType)value;
      break;
    case FillingMode:
      fillingMode = (FillingType)value;
      break;
    case EraseCount:
      fillingEraseCount = (uint32_t)value;
      break;
    case GCMode:
      gcMode = (GCType)value;
      break;
    case VictimSelectionPolicy:
      gcBlockSelection = (VictimSelectionMode)value;
      break;
    case SamplingFactor:
      dChoiceParam = value;
      break;
    case SuperpageAllocation:
      superpageAllocation = (uint8_t)value;
      break;
    case ForegroundBlockEraseLevel:
      fgcBlockEraseLevel = (Granularity)value;
      break;
    case BackgroundBlockEraseLevel:
      bgcBlockEraseLevel = (Granularity)value;
      break;
    case IdleTimeForBackgroundGC:
      bgcIdletime = value;
      break;
    case WearLevelingMode:
      wlMode = (WearLevelingType)value;
      break;
    case ReadReclaimMode:
      rrMode = (ReadReclaimType)value;
      break;
    default:
      ret = false;
      break;
  }

  return ret;
}

bool Config::writeFloat(uint32_t idx, float value) noexcept {
  bool ret = true;

  switch (idx) {
    case OverProvisioningRatio:
      overProvision = value;
      break;
    case FillRatio:
      fillRatio = value;
      break;
    case InvalidFillRatio:
      invalidFillRatio = value;
      break;
    case ForegroundGCThreshold:
      fgcThreshold = value;
      break;
    case BackgroundGCThreshold:
      bgcThreshold = value;
      break;
    case StaticWearLevelingThreshold:
      staticWearLevelingThreshold = value;
      break;
    default:
      ret = false;
      break;
  }

  return ret;
}

bool Config::writeBoolean(uint32_t idx, bool value) noexcept {
  bool ret = true;

  switch (idx) {
    case MergeReadModifyWrite:
      mergeRMW = value;
      break;
    default:
      ret = false;
      break;
  }

  return ret;
}

}  // namespace SimpleSSD::FTL
