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
const char NAME_BAD_BLOCK_THRESHOLD[] = "EraseThreshold";
const char NAME_FILLING_MODE[] = "FillingMode";
const char NAME_FILL_RATIO[] = "FillRatio";
const char NAME_INVALID_PAGE_RATIO[] = "InvalidFillRatio";
const char NAME_GC_EVICT_POLICY[] = "VictimSelectionPolicy";
const char NAME_GC_D_CHOICE_PARAM[] = "DChoiceParam";
const char NAME_GC_THRESHOLD[] = "GCThreshold";
const char NAME_GC_MODE[] = "GCMode";
const char NAME_GC_RECLAIM_BLOCK[] = "GCReclaimBlocks";
const char NAME_GC_RECLAIM_THRESHOLD[] = "GCReclaimThreshold";
const char NAME_USE_SUPERPAGE[] = "UseSuperpage";
const char NAME_SUPERPAGE_ALLOCATION[] = "SuperpageAllocation";
const char NAME_PARTIAL_MAPPING_TABLE_RATIO[] = "VLTableRatio";
const char NAME_MERGE_BEGIN_THRESHOLD[] = "MergeBeginThreshold";
const char NAME_MERGE_END_THRESHOLD[] = "MergeEndThreshold";

Config::Config() {
  mappingMode = MappingType::PageLevelFTL;
  overProvision = 0.2f;
  eraseThreshold = 100000;
  fillingMode = FillingType::SequentialSequential;
  fillRatio = 1.f;
  invalidFillRatio = 0.f;

  gcBlockSelection = VictimSelectionMode::Greedy;
  dChoiceParam = 3;
  gcThreshold = 0.05f;
  gcMode = GCBlockReclaimMode::ByCount;
  gcReclaimBlocks = 1;
  gcReclaimThreshold = 0.1f;
  useSuperpage = false;
  superpageAllocation = FIL::PageAllocation::Channel;
  pmTableRatio = 0.3f;
  mergeBeginThreshold = 0.1f;
  mergeEndThreshold = 0.2f;
}

void Config::loadFrom(pugi::xml_node &section) {
  for (auto node = section.first_child(); node; node = node.next_sibling()) {
    auto name = node.attribute("name").value();

    LOAD_NAME_UINT_TYPE(node, NAME_MAPPING_MODE, MappingType, mappingMode);
    LOAD_NAME_FLOAT(node, NAME_OVERPROVISION_RATIO, overProvision);
    LOAD_NAME_UINT(node, NAME_BAD_BLOCK_THRESHOLD, eraseThreshold);

    if (strcmp(name, "warmup") == 0 && isSection(node)) {
      for (auto node2 = node.first_child(); node2;
           node2 = node2.next_sibling()) {
        LOAD_NAME_UINT_TYPE(node2, NAME_FILLING_MODE, FillingType, fillingMode);
        LOAD_NAME_FLOAT(node2, NAME_FILL_RATIO, fillRatio);
        LOAD_NAME_FLOAT(node2, NAME_INVALID_PAGE_RATIO, invalidFillRatio);
      }
    }
    else if (strcmp(name, "gc") == 0 && isSection(node)) {
      for (auto node2 = node.first_child(); node2;
           node2 = node2.next_sibling()) {
        LOAD_NAME_UINT_TYPE(node2, NAME_GC_EVICT_POLICY, VictimSelectionMode,
                            gcBlockSelection);
        LOAD_NAME_UINT(node2, NAME_GC_D_CHOICE_PARAM, dChoiceParam);
        LOAD_NAME_FLOAT(node2, NAME_GC_THRESHOLD, gcThreshold);
        LOAD_NAME_UINT_TYPE(node2, NAME_GC_MODE, GCBlockReclaimMode, gcMode);
        LOAD_NAME_UINT(node2, NAME_GC_RECLAIM_BLOCK, gcReclaimBlocks);
        LOAD_NAME_UINT(node2, NAME_GC_RECLAIM_THRESHOLD, gcReclaimThreshold);
      }
    }
    else if (strcmp(name, "common") == 0 && isSection(node)) {
      for (auto node2 = node.first_child(); node2;
           node2 = node2.next_sibling()) {
        LOAD_NAME_BOOLEAN(node2, NAME_USE_SUPERPAGE, useSuperpage);
        LOAD_NAME_STRING(node2, NAME_SUPERPAGE_ALLOCATION, superpage);
      }
    }
    else if (strcmp(name, "vlftl") == 0 && isSection(node)) {
      for (auto node2 = node.first_child(); node2;
           node2 = node2.next_sibling()) {
        LOAD_NAME_FLOAT(node2, NAME_PARTIAL_MAPPING_TABLE_RATIO, pmTableRatio);
        LOAD_NAME_FLOAT(node2, NAME_MERGE_BEGIN_THRESHOLD, mergeBeginThreshold);
        LOAD_NAME_FLOAT(node2, NAME_MERGE_END_THRESHOLD, mergeEndThreshold);
      }
    }
  }
}

void Config::storeTo(pugi::xml_node &section) {
  pugi::xml_node node;

  STORE_NAME_UINT(section, NAME_MAPPING_MODE, mappingMode);
  STORE_NAME_FLOAT(section, NAME_OVERPROVISION_RATIO, overProvision);
  STORE_NAME_UINT(section, NAME_BAD_BLOCK_THRESHOLD, eraseThreshold);

  STORE_SECTION(section, "warmup", node);
  STORE_NAME_UINT(node, NAME_FILLING_MODE, fillingMode);
  STORE_NAME_FLOAT(node, NAME_FILL_RATIO, fillRatio);
  STORE_NAME_FLOAT(node, NAME_INVALID_PAGE_RATIO, invalidFillRatio);

  STORE_SECTION(section, "gc", node);
  STORE_NAME_UINT(node, NAME_GC_EVICT_POLICY, gcBlockSelection);
  STORE_NAME_UINT(node, NAME_GC_D_CHOICE_PARAM, dChoiceParam);
  STORE_NAME_FLOAT(node, NAME_GC_THRESHOLD, gcThreshold);
  STORE_NAME_UINT(node, NAME_GC_MODE, gcMode);
  STORE_NAME_UINT(node, NAME_GC_RECLAIM_BLOCK, gcReclaimBlocks);
  STORE_NAME_UINT(node, NAME_GC_RECLAIM_THRESHOLD, gcReclaimThreshold);

  STORE_SECTION(section, "common", node);
  STORE_NAME_BOOLEAN(node, NAME_USE_SUPERPAGE, useSuperpage);
  STORE_NAME_STRING(node, NAME_SUPERPAGE_ALLOCATION, superpage);

  STORE_SECTION(section, "vlftl", node);
  STORE_NAME_FLOAT(node, NAME_PARTIAL_MAPPING_TABLE_RATIO, pmTableRatio);
  STORE_NAME_FLOAT(node, NAME_MERGE_BEGIN_THRESHOLD, mergeBeginThreshold);
  STORE_NAME_FLOAT(node, NAME_MERGE_END_THRESHOLD, mergeEndThreshold);
}

void Config::update() {
  panic_if((uint8_t)mappingMode > 2, "Invalid MappingMode.");
  panic_if((uint8_t)fillingMode > 2, "Invalid FillingMode.");
  panic_if((uint8_t)gcBlockSelection > 3, "Invalid VictimSelectionPolicy.");
  panic_if((uint8_t)gcMode > 1, "Invalid GCMode.");

  panic_if(gcMode == GCBlockReclaimMode::ByCount && gcReclaimBlocks == 0,
           "Invalid GCReclaimBlocks.");
  panic_if(
      gcMode == GCBlockReclaimMode::ByRatio && gcReclaimThreshold < gcThreshold,
      "Invalid GCReclaimThreshold.");

  panic_if(fillRatio < 0.f || fillRatio > 1.f, "Invalid FillingRatio.");
  panic_if(invalidFillRatio < 0.f || invalidFillRatio > 1.f,
           "Invalid InvalidPageRatio.");
}

uint64_t Config::readUint(uint32_t idx) {
  uint64_t ret = 0;

  switch (idx) {
    case MappingMode:
      ret = (uint64_t)mappingMode;
      break;
    case EraseThreshold:
      ret = eraseThreshold;
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
    case GCMode:
      ret = (uint64_t)gcMode;
      break;
    case GCReclaimBlocks:
      ret = gcReclaimBlocks;
      break;
    case SuperpageAllocation:
      ret = (uint64_t)superpageAllocation;
      break;
  }

  return ret;
}

float Config::readFloat(uint32_t idx) {
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
    case GCThreshold:
      ret = gcThreshold;
      break;
    case GCReclaimThreshold:
      ret = gcReclaimThreshold;
      break;
    case VLTableRatio:
      ret = pmTableRatio;
      break;
    case MergeBeginThreshold:
      ret = mergeBeginThreshold;
      break;
    case MergeEndThreshold:
      ret = mergeEndThreshold;
      break;
  }

  return ret;
}

bool Config::readBoolean(uint32_t idx) {
  bool ret = false;

  switch (idx) {
    case UseSuperpage:
      ret = useSuperpage;
      break;
  }

  return ret;
}

bool Config::writeUint(uint32_t idx, uint64_t value) {
  bool ret = true;

  switch (idx) {
    case MappingMode:
      mappingMode = (MappingType)value;
      break;
    case EraseThreshold:
      eraseThreshold = value;
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
    case GCMode:
      gcMode = (GCBlockReclaimMode)value;
      break;
    case GCReclaimBlocks:
      gcReclaimBlocks = value;
      break;
    case SuperpageAllocation:
      superpageAllocation = (FIL::PageAllocation)value;
      break;
    default:
      ret = false;
      break;
  }

  return ret;
}

bool Config::writeFloat(uint32_t idx, float value) {
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
    case GCThreshold:
      gcThreshold = value;
      break;
    case GCReclaimThreshold:
      gcReclaimThreshold = value;
      break;
    case VLTableRatio:
      pmTableRatio = value;
      break;
    case MergeBeginThreshold:
      mergeBeginThreshold = value;
      break;
    case MergeEndThreshold:
      mergeEndThreshold = value;
      break;
    default:
      ret = false;
      break;
  }

  return ret;
}

bool Config::writeBoolean(uint32_t idx, bool value) {
  bool ret = true;

  switch (idx) {
    case UseSuperpage:
      useSuperpage = value;
      break;
    default:
      ret = false;
      break;
  }

  return ret;
}

}  // namespace SimpleSSD::FTL
