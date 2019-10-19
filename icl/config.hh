// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_ICL_CONFIG_HH__
#define __SIMPLESSD_ICL_CONFIG_HH__

#include "sim/base_config.hh"

namespace SimpleSSD::ICL {

class Config : public BaseConfig {
 public:
  enum Key : uint32_t {
    EnableReadCache,
    EnablePrefetch,
    EnableWriteCache,
    PrefetchMode,
    PrefetchCount,
    PrefetchRatio,
    CacheMode,
    CacheSize,
    EvictPolicy,
    EvictMode,
  };

  enum class Mode : uint8_t {
    RingBuffer,
  };

  enum class EvictModeType : uint8_t {
    Random,
    FIFO,
    LRU,
  };

  enum class Granularity : uint8_t {
    SuperpageLevel,
    AllLevel,
  };

 private:
  bool readCaching;
  bool readPrefetch;
  bool writeCaching;
  Granularity prefetchMode;
  uint64_t prefetchCount;
  uint64_t prefetchRatio;
  Mode mode;
  uint64_t cacheSize;
  EvictModeType evictPolicy;
  Granularity evictMode;

 public:
  Config();
  ~Config();

  const char *getSectionName() override { return "icl"; }

  void loadFrom(pugi::xml_node &) override;
  void storeTo(pugi::xml_node &) override;
  void update() override;

  uint64_t readUint(uint32_t) override;
  bool readBoolean(uint32_t) override;
  bool writeUint(uint32_t, uint64_t) override;
  bool writeBoolean(uint32_t, bool) override;
};

}  // namespace SimpleSSD::ICL

#endif
