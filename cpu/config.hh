// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_CPU_CONFIG_HH__
#define __SIMPLESSD_CPU_CONFIG_HH__

#include "sim/base_config.hh"

namespace SimpleSSD::CPU {

/**
 * \brief CPUConfig object declaration
 *
 * Stores CPU configurations.
 */
class Config : public BaseConfig {
 public:
  enum Key : uint32_t {
    Clock,
    UseDedicatedCore,
    HILCore,
    ICLCore,
    FTLCore,
  };

 private:
  uint64_t clock;
  bool useDedicatedCore;
  uint32_t hilCore;
  uint32_t iclCore;
  uint32_t ftlCore;

 public:
  Config();

  const char *getSectionName() noexcept override { return "cpu"; }

  void loadFrom(pugi::xml_node &) noexcept override;
  void storeTo(pugi::xml_node &) noexcept override;
  void update() noexcept override;

  uint64_t readUint(uint32_t) const noexcept override;
  bool readBoolean(uint32_t) const noexcept override;
  bool writeUint(uint32_t, uint64_t) noexcept override;
  bool writeBoolean(uint32_t, bool) noexcept override;
};

}  // namespace SimpleSSD::CPU

#endif
