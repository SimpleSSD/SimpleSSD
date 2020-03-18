// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "icl/config.hh"

namespace SimpleSSD::ICL {

const char NAME_CACHE_MODE[] = "CacheMode";
const char NAME_CACHE_SIZE[] = "CacheSize";
const char NAME_USE_READ_PREFETCH[] = "EnableReadPrefetch";
const char NAME_PREFETCH_MODE[] = "PrefetchMode";
const char NAME_PREFETCH_COUNT[] = "PrefetchCount";
const char NAME_PREFETCH_RATIO[] = "PrefetchRatio";
const char NAME_EVICT_POLICY[] = "EvictPolicy";
const char NAME_EVICT_MODE[] = "EvictMode";
const char NAME_EVICT_THRESHOLD[] = "EvictThreshold";
const char NAME_WAY_SIZE[] = "WaySize";

Config::Config() {
  readPrefetch = false;
  prefetchCount = 1;
  prefetchRatio = 4;
  prefetchMode = Granularity::SecondLevel;
  mode = Mode::SetAssociative;
  cacheSize = 33554432;
  evictPolicy = EvictPolicyType::LRU;
  evictMode = Granularity::SecondLevel;
  evictThreshold = 0.7f;
  waySize = 8;
}

void Config::loadFrom(pugi::xml_node &section) {
  for (auto node = section.first_child(); node; node = node.next_sibling()) {
    auto name = node.attribute("name").value();

    LOAD_NAME_UINT_TYPE(node, NAME_CACHE_MODE, Mode, mode);
    LOAD_NAME_UINT(node, NAME_CACHE_SIZE, cacheSize);

    if (strcmp(name, "prefetch") == 0 && isSection(node)) {
      for (auto node2 = node.first_child(); node2;
           node2 = node2.next_sibling()) {
        LOAD_NAME_BOOLEAN(node2, NAME_USE_READ_PREFETCH, readPrefetch);
        LOAD_NAME_UINT(node2, NAME_PREFETCH_COUNT, prefetchCount);
        LOAD_NAME_UINT(node2, NAME_PREFETCH_RATIO, prefetchRatio);
        LOAD_NAME_UINT_TYPE(node2, NAME_PREFETCH_MODE, Granularity,
                            prefetchMode);
      }
    }
    else if (strcmp(name, "eviction") == 0 && isSection(node)) {
      for (auto node2 = node.first_child(); node2;
           node2 = node2.next_sibling()) {
        LOAD_NAME_UINT_TYPE(node2, NAME_EVICT_POLICY, EvictPolicyType,
                            evictPolicy);
        LOAD_NAME_UINT_TYPE(node2, NAME_EVICT_MODE, Granularity, evictMode);
        LOAD_NAME_FLOAT(node2, NAME_EVICT_THRESHOLD, evictThreshold);
      }
    }
    else if (strcmp(name, "setassoc") == 0 && isSection(node)) {
      for (auto node2 = node.first_child(); node2;
           node2 = node2.next_sibling()) {
        LOAD_NAME_UINT_TYPE(node2, NAME_WAY_SIZE, uint32_t, waySize);
      }
    }
  }
}

void Config::storeTo(pugi::xml_node &section) {
  pugi::xml_node node;

  STORE_NAME_UINT(section, NAME_CACHE_MODE, mode);
  STORE_NAME_UINT(section, NAME_CACHE_SIZE, cacheSize);

  STORE_SECTION(section, "prefetch", node);
  STORE_NAME_BOOLEAN(node, NAME_USE_READ_PREFETCH, readPrefetch);
  STORE_NAME_UINT(node, NAME_PREFETCH_COUNT, prefetchCount);
  STORE_NAME_UINT(node, NAME_PREFETCH_RATIO, prefetchRatio);
  STORE_NAME_UINT(node, NAME_PREFETCH_MODE, prefetchMode);

  STORE_SECTION(section, "eviction", node);
  STORE_NAME_UINT(node, NAME_EVICT_POLICY, evictPolicy);
  STORE_NAME_UINT(node, NAME_EVICT_MODE, evictMode);
  STORE_NAME_FLOAT(node, NAME_EVICT_THRESHOLD, evictThreshold);

  STORE_SECTION(section, "setassoc", node);
  STORE_NAME_UINT(node, NAME_WAY_SIZE, waySize);
}

void Config::update() {
  panic_if(prefetchCount == 0, "Invalid PrefetchCount.");
  panic_if(prefetchRatio == 0, "Invalid PrefetchRatio.");
  panic_if((uint8_t)prefetchMode > 1, "Invalid PrefetchMode.");
  panic_if(evictThreshold < 0.f || evictThreshold >= 1.f,
           "Invalid EvictThreshold.");
}

uint64_t Config::readUint(uint32_t idx) {
  uint64_t ret = 0;

  switch (idx) {
    case PrefetchMode:
      ret = (uint64_t)prefetchMode;
      break;
    case PrefetchCount:
      ret = prefetchCount;
      break;
    case PrefetchRatio:
      ret = prefetchRatio;
      break;
    case CacheMode:
      ret = (uint64_t)mode;
      break;
    case CacheSize:
      ret = cacheSize;
      break;
    case EvictPolicy:
      ret = (uint64_t)evictPolicy;
      break;
    case EvictGranularity:
      ret = (uint64_t)evictMode;
      break;
    case CacheWaySize:
      ret = waySize;
      break;
  }

  return ret;
}

float Config::readFloat(uint32_t idx) {
  switch (idx) {
    case EvictThreshold:
      return evictThreshold;
      break;
  }

  return 0.f;
}

bool Config::readBoolean(uint32_t idx) {
  bool ret = false;

  switch (idx) {
    case EnablePrefetch:
      ret = readPrefetch;
      break;
  }

  return ret;
}

bool Config::writeUint(uint32_t idx, uint64_t value) {
  bool ret = true;

  switch (idx) {
    case PrefetchMode:
      prefetchMode = (Granularity)value;
      break;
    case PrefetchCount:
      prefetchCount = value;
      break;
    case PrefetchRatio:
      prefetchRatio = value;
      break;
    case CacheMode:
      mode = (Mode)value;
      break;
    case CacheSize:
      cacheSize = value;
      break;
    case EvictPolicy:
      evictPolicy = (EvictPolicyType)value;
      break;
    case EvictGranularity:
      evictMode = (Granularity)value;
      break;
    case CacheWaySize:
      waySize = (uint32_t)value;
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
    case EvictThreshold:
      evictThreshold = value;
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
    case EnablePrefetch:
      readPrefetch = value;
      break;
    default:
      ret = false;
      break;
  }

  return ret;
}

}  // namespace SimpleSSD::ICL
