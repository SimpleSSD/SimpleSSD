// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "icl/config.hh"

namespace SimpleSSD::ICL {

const char NAME_USE_READ_CACHE[] = "EnableReadCache";
const char NAME_USE_READ_PREFETCH[] = "EnableReadPrefetch";
const char NAME_USE_WRITE_CACHE[] = "EnableWriteCache";
const char NAME_PREFETCH_MODE[] = "PrefetchMode";
const char NAME_PREFETCH_COUNT[] = "PrefetchCount";
const char NAME_PREFETCH_RATIO[] = "PrefetchRatio";
const char NAME_CACHE_MODE[] = "CacheMode";
const char NAME_CACHE_SIZE[] = "CacheSize";
const char NAME_EVICT_POLICY[] = "EvictPolicy";
const char NAME_EVICT_MODE[] = "EvictMode";

Config::Config() {
  readCaching = false;
  writeCaching = true;
  readPrefetch = false;
  prefetchCount = 1;
  prefetchRatio = 0.5;
  prefetchMode = Granularity::AllLevel;
  mode = Mode::SetAssociative;
  cacheSize = 33554432;
  evictPolicy = EvictModeType::LRU;
  evictMode = Granularity::AllLevel;
}

void Config::loadFrom(pugi::xml_node &section) {
  for (auto node = section.first_child(); node; node = node.next_sibling()) {
    auto name = node.attribute("name").value();

    LOAD_NAME_BOOLEAN(node, NAME_USE_READ_CACHE, readCaching);
    LOAD_NAME_BOOLEAN(node, NAME_USE_READ_PREFETCH, writeCaching);
    LOAD_NAME_BOOLEAN(node, NAME_USE_WRITE_CACHE, readPrefetch);
    LOAD_NAME_BOOLEAN(node, NAME_PREFETCH_RATIO, prefetchCount);
    LOAD_NAME_BOOLEAN(node, NAME_PREFETCH_COUNT, prefetchRatio);
    LOAD_NAME_UINT_TYPE(node, NAME_PREFETCH_MODE, Granularity, prefetchMode);
    LOAD_NAME_UINT_TYPE(node, NAME_CACHE_MODE, Mode, mode);
    LOAD_NAME_UINT(node, NAME_CACHE_SIZE, cacheSize);
    LOAD_NAME_UINT_TYPE(node, NAME_EVICT_POLICY, EvictModeType, evictPolicy);
    LOAD_NAME_UINT_TYPE(node, NAME_EVICT_MODE, Granularity, evictMode);
  }
}  // namespace SimpleSSD::ICL

void Config::storeTo(pugi::xml_node &section) {
  STORE_NAME_BOOLEAN(section, NAME_USE_READ_CACHE, readCaching);
  STORE_NAME_BOOLEAN(section, NAME_USE_READ_PREFETCH, writeCaching);
  STORE_NAME_BOOLEAN(section, NAME_USE_WRITE_CACHE, readPrefetch);
  STORE_NAME_BOOLEAN(section, NAME_PREFETCH_RATIO, prefetchCount);
  STORE_NAME_BOOLEAN(section, NAME_PREFETCH_COUNT, prefetchRatio);
  STORE_NAME_UINT(section, NAME_PREFETCH_MODE, prefetchMode);
  STORE_NAME_UINT(section, NAME_CACHE_MODE, mode);
  STORE_NAME_UINT(section, NAME_CACHE_SIZE, cacheSize);
  STORE_NAME_UINT(section, NAME_EVICT_POLICY, evictPolicy);
  STORE_NAME_UINT(section, NAME_EVICT_MODE, evictMode);
}

void Config::update() {
  panic_if(prefetchCount == 0, "Invalid PrefetchCount.");
  panic_if(prefetchRatio == 0, "Invalid PrefetchRatio.");
  panic_if((uint8_t)prefetchMode > 1, "Invalid PrefetchMode.");
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
    case EvictMode:
      ret = (uint64_t)evictMode;
      break;
  }

  return ret;
}

bool Config::readBoolean(uint32_t idx) {
  bool ret = false;

  switch (idx) {
    case EnableReadCache:
      ret = readCaching;
      break;
    case EnablePrefetch:
      ret = readPrefetch;
      break;
    case EnableWriteCache:
      ret = writeCaching;
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
      evictPolicy = (EvictModeType)value;
      break;
    case EvictMode:
      evictMode = (Granularity)value;
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
    case EnableReadCache:
      readCaching = value;
      break;
    case EnablePrefetch:
      readPrefetch = value;
      break;
    case EnableWriteCache:
      writeCaching = value;
      break;
    default:
      ret = false;
      break;
  }

  return ret;
}

}  // namespace SimpleSSD::ICL
