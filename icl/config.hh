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
    CacheMode,
    CacheSize,
    EnablePrefetch,
    PrefetchMode,
    PrefetchCount,
    PrefetchRatio,
    EvictPolicy,
    EvictGranularity,
    EvictThreshold,
    CacheWaySize,
  };

  enum class Mode : uint8_t {
    None,
    SetAssociative,
    RingBuffer,
  };

  enum class EvictPolicyType : uint8_t {
    FIFO,
    LRU,
  };

  enum class Granularity : uint8_t {
    FirstLevel,
    SecondLevel,
    ThirdLevel,
    AllLevel,
  };

 private:
  Mode mode;
  bool readPrefetch;
  Granularity prefetchMode;
  EvictPolicyType evictPolicy;
  float evictThreshold;
  uint64_t prefetchCount;
  uint64_t prefetchRatio;
  uint64_t cacheSize;
  Granularity evictMode;
  uint32_t waySize;

 public:
  Config();

  const char *getSectionName() noexcept override { return "icl"; }

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

}  // namespace SimpleSSD::ICL

#endif
