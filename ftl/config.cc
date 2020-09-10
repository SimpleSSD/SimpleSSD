// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "ftl/config.hh"

namespace SimpleSSD::FTL {

const char NAME_MAPPING_MODE[] = "MappingMode";
const char NAME_OVERPROVISION_RATIO[] = "OverProvisioningRatio";
const char NAME_FILLING_MODE[] = "FillingMode";
const char NAME_FILL_RATIO[] = "FillRatio";
const char NAME_INVALID_PAGE_RATIO[] = "InvalidFillRatio";
const char NAME_GC_EVICT_POLICY[] = "VictimSelectionPolicy";
const char NAME_GC_D_CHOICE_PARAM[] = "DChoiceParam";
const char NAME_FGC_THRESHOLD[] = "FGCThreshold";
const char NAME_BGC_THRESHOLD[] = "BGCThreshold";
const char NAME_BGC_IDLETIME[] = "BGCIdleTime";
const char NAME_SUPERPAGE_ALLOCATION[] = "SuperpageAllocation";
const char NAME_MERGE_RMW[] = "MergeReadModifyWrite";
const char NAME_MERGE_THRESHOLD[] = "MergeThreshold";
const char NAME_DEMAND_PAGING[] = "DemandPaging";

Config::Config() {
  mappingMode = MappingType::PageLevelFTL;
  overProvision = 0.2f;
  fillingMode = FillingType::SequentialSequential;
  fillRatio = 1.f;
  invalidFillRatio = 0.f;

  mergeRMW = false;
  demandPaging = false;

  gcBlockSelection = VictimSelectionMode::Greedy;
  dChoiceParam = 3;
  fgcThreshold = 0.05f;
  bgcThreshold = 0.1f;
  bgcIdletime = 5000000000000ul;
  superpageAllocation = FIL::PageAllocation::None;
}

void Config::loadFrom(pugi::xml_node &section) noexcept {
  for (auto node = section.first_child(); node; node = node.next_sibling()) {
    auto name = node.attribute("name").value();

    LOAD_NAME_UINT_TYPE(node, NAME_MAPPING_MODE, MappingType, mappingMode);

    if (strcmp(name, "gc") == 0 && isSection(node)) {
      for (auto node2 = node.first_child(); node2;
           node2 = node2.next_sibling()) {
        LOAD_NAME_UINT_TYPE(node2, NAME_GC_EVICT_POLICY, VictimSelectionMode,
                            gcBlockSelection);
        LOAD_NAME_UINT(node2, NAME_GC_D_CHOICE_PARAM, dChoiceParam);
        LOAD_NAME_FLOAT(node2, NAME_FGC_THRESHOLD, fgcThreshold);
        LOAD_NAME_FLOAT(node2, NAME_BGC_THRESHOLD, bgcThreshold);
        LOAD_NAME_FLOAT(node2, NAME_BGC_IDLETIME, bgcIdletime);
      }
    }
    else if (strcmp(name, "common") == 0 && isSection(node)) {
      for (auto node2 = node.first_child(); node2;
           node2 = node2.next_sibling()) {
        auto name2 = node2.attribute("name").value();

        LOAD_NAME_FLOAT(node2, NAME_OVERPROVISION_RATIO, overProvision);
        LOAD_NAME_STRING(node2, NAME_SUPERPAGE_ALLOCATION, superpage);
        LOAD_NAME_BOOLEAN(node2, NAME_MERGE_RMW, mergeRMW);
        LOAD_NAME_BOOLEAN(node2, NAME_DEMAND_PAGING, demandPaging);

        if (strcmp(name2, "warmup") == 0 && isSection(node)) {
          for (auto node3 = node2.first_child(); node3;
               node3 = node3.next_sibling()) {
            LOAD_NAME_UINT_TYPE(node3, NAME_FILLING_MODE, FillingType,
                                fillingMode);
            LOAD_NAME_FLOAT(node3, NAME_FILL_RATIO, fillRatio);
            LOAD_NAME_FLOAT(node3, NAME_INVALID_PAGE_RATIO, invalidFillRatio);
          }
        }
      }
    }
  }
}

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

  STORE_SECTION(section, "gc", node);
  STORE_NAME_UINT(node, NAME_GC_EVICT_POLICY, gcBlockSelection);
  STORE_NAME_UINT(node, NAME_GC_D_CHOICE_PARAM, dChoiceParam);
  STORE_NAME_FLOAT(node, NAME_FGC_THRESHOLD, fgcThreshold);
  STORE_NAME_FLOAT(node, NAME_BGC_THRESHOLD, bgcThreshold);
  STORE_NAME_FLOAT(node, NAME_BGC_IDLETIME, bgcIdletime);

  STORE_SECTION(section, "common", node);
  STORE_NAME_FLOAT(node, NAME_OVERPROVISION_RATIO, overProvision);
  STORE_NAME_STRING(node, NAME_SUPERPAGE_ALLOCATION, superpage);
  STORE_NAME_BOOLEAN(node, NAME_MERGE_RMW, mergeRMW);
  STORE_NAME_BOOLEAN(node, NAME_DEMAND_PAGING, demandPaging);

  STORE_SECTION(node, "warmup", node2);
  STORE_NAME_UINT(node2, NAME_FILLING_MODE, fillingMode);
  STORE_NAME_FLOAT(node2, NAME_FILL_RATIO, fillRatio);
  STORE_NAME_FLOAT(node2, NAME_INVALID_PAGE_RATIO, invalidFillRatio);
}

void Config::update() noexcept {
  panic_if((uint8_t)mappingMode > 2, "Invalid MappingMode.");
  panic_if((uint8_t)fillingMode > 2, "Invalid FillingMode.");
  panic_if((uint8_t)gcBlockSelection > 3, "Invalid VictimSelectionPolicy.");

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
    case VictimSelectionPolicy:
      ret = (uint64_t)gcBlockSelection;
      break;
    case DChoiceParam:
      ret = dChoiceParam;
      break;
    case SuperpageAllocation:
      ret = superpageAllocation;
      break;
    case BGCIdleTime:
      ret = bgcIdletime;
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
    case FGCThreshold:
      ret = fgcThreshold;
      break;
    case BGCThreshold:
      ret = bgcThreshold;
      break;
  }

  return ret;
}

bool Config::readBoolean(uint32_t idx) const noexcept {
  switch (idx) {
    case MergeReadModifyWrite:
      return mergeRMW;
      break;
    case DemandPaging:
      return demandPaging;
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
    case VictimSelectionPolicy:
      gcBlockSelection = (VictimSelectionMode)value;
      break;
    case DChoiceParam:
      dChoiceParam = value;
      break;
    case SuperpageAllocation:
      superpageAllocation = (uint8_t)value;
      break;
    case BGCIdleTime:
      bgcIdletime = value;
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
    case FGCThreshold:
      fgcThreshold = value;
      break;
    case BGCThreshold:
      bgcThreshold = value;
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
    case DemandPaging:
      demandPaging = value;
      break;
    default:
      ret = false;
      break;
  }

  return ret;
}

}  // namespace SimpleSSD::FTL
