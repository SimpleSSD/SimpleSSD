// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "mem/mem_config.hh"

#include <cstring>

namespace SimpleSSD {

const char NAME_MODEL[] = "Model";
const char NAME_WAY[] = "Way";
const char NAME_LINE_SIZE[] = "LineSize";
const char NAME_SIZE[] = "Size";
const char NAME_LATENCY[] = "Latency";
const char NAME_CHANNEL[] = "Channel";
const char NAME_RANK[] = "Rank";
const char NAME_BANK[] = "Bank";
const char NAME_CHIP[] = "Chip";
const char NAME_BUS_WIDTH[] = "BusWidth";
const char NAME_BURST[] = "BurstLength";
const char NAME_CHIP_SIZE[] = "ChipSize";

MemConfig::MemConfig() {}

MemConfig::~MemConfig() {}

void MemConfig::loadCache(pugi::xml_node &section, CacheParameter *param) {
  for (auto node = section.first_child(); node; node = node.next_sibling()) {
    LOAD_NAME_UINT(node, NAME_MODEL, param->model, 0);  // TODO FILL DEF
    LOAD_NAME_UINT(node, NAME_WAY, param->way, 8);
    LOAD_NAME_UINT(node, NAME_LINE_SIZE, param->lineSize, 64);
    LOAD_NAME_UINT(node, NAME_SIZE, param->size, 32768);
    LOAD_NAME_UINT(node, NAME_LATENCY, param->latency, 10000);
  }
}

void MemConfig::loadDRAMStructure(pugi::xml_node &section,
                                  DRAMParameter *param) {
  for (auto node = section.first_child(); node; node = node.next_sibling()) {
    LOAD_NAME_UINT(node, NAME_CHANNEL, param->channel, 1);
    LOAD_NAME_UINT(node, NAME_RANK, param->rank, 1);
    LOAD_NAME_UINT(node, NAME_BANK, param->bank, 8);
    LOAD_NAME_UINT(node, NAME_CHIP, param->chip, 1);
    LOAD_NAME_UINT(node, NAME_BUS_WIDTH, param->width, 32);
    LOAD_NAME_UINT(node, NAME_BURST, param->burst, 8);
    LOAD_NAME_UINT(node, NAME_CHIP_SIZE, param->chipsize, 1073741824);
  }
}

void MemConfig::storeCache(pugi::xml_node &section, CacheParameter *param) {
  STORE_NAME_UINT(section, NAME_MODEL, param->model);
  STORE_NAME_UINT(section, NAME_WAY, param->way);
  STORE_NAME_UINT(section, NAME_LINE_SIZE, param->lineSize);
  STORE_NAME_UINT(section, NAME_SIZE, param->size);
  STORE_NAME_UINT(section, NAME_LATENCY, param->latency);
}

void MemConfig::storeDRAMStructure(pugi::xml_node &section,
                                   DRAMParameter *param) {
  STORE_NAME_UINT(section, NAME_CHANNEL, param->channel);
  STORE_NAME_UINT(section, NAME_RANK, param->rank);
  STORE_NAME_UINT(section, NAME_BANK, param->bank);
  STORE_NAME_UINT(section, NAME_CHIP, param->chip);
  STORE_NAME_UINT(section, NAME_BUS_WIDTH, param->width);
  STORE_NAME_UINT(section, NAME_BURST, param->burst);
  STORE_NAME_UINT(section, NAME_CHIP_SIZE, param->chipsize);
}

void MemConfig::loadFrom(pugi::xml_node &section) {
  for (auto node = section.first_child(); node; node = node.next_sibling()) {
    auto name = node.attribute("name").value();

    if (strcmp(name, "level1") == 0) {
      loadCache(node, &level1);
    }
    else if (strcmp(name, "level2") == 0) {
      loadCache(node, &level2);
    }
    else if (strcmp(name, "dram") == 0) {
      for (auto node2 = node.first_child(); node2;
           node2 = node2.next_sibling()) {
        auto name2 = node2.attribute("name").value();

        if (strcmp(name2, "struct") == 0) {
          loadDRAMStructure(node2, &dram);
        }
        else if (strcmp(name2, "timing") == 0) {
          loadDRAMTiming(node2, &timing);
        }
        else if (strcmp(name2, "power") == 0) {
          loadDRAMPower(node2, &power);
        }

        LOAD_NAME_UINT(node2, NAME_MODEL, dramModel, 0);  // TODO FILL DEF
      }
    }
  }
}

void MemConfig::storeTo(pugi::xml_node &section) {
  pugi::xml_node node, node2;

  STORE_SECTION(section, "level1", node);
  storeCache(node, &level1);

  STORE_SECTION(section, "level2", node);
  storeCache(node, &level2);

  STORE_SECTION(section, "dram", node);
  STORE_NAME_UINT(node, NAME_MODEL, dramModel);

  STORE_SECTION(node, "struct", node2);
  storeDRAMStructure(node2, &dram);

  STORE_SECTION(node, "timing", node2);
  storeDRAMTiming(node2, &timing);

  STORE_SECTION(node, "power", node2);
  storeDRAMPower(node2, &power);
}

void MemConfig::update() {
  // Fill cache param
  level1.set = level1.size / level1.way / level1.lineSize;
  level2.set = level2.size / level2.way / level2.lineSize;

  // Validate cache
  panic_if(level1.set * level1.way * level1.lineSize != level1.size,
           "Level 1 cache size is not aligned");
  panic_if(level2.set * level2.way * level2.lineSize != level2.size,
           "Level 2 cache size is not aligned");
}

}  // namespace SimpleSSD
