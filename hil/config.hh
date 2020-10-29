// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_HIL_CONFIG_HH__
#define __SIMPLESSD_HIL_CONFIG_HH__

#include "sim/base_config.hh"
#include "util/interface.hh"

namespace SimpleSSD::HIL {

/**
 * \brief HILConfig object declaration
 *
 * Stores HIL configurations.
 */
class Config : public BaseConfig {
 public:
  enum Key : uint32_t {
    WorkInterval,
    RequestQueueSize,
    PCIeGeneration,
    PCIeLane,
    SATAGeneration,
    MPHYMode,
    MPHYLane,
    NVMeMaxSQ,
    NVMeMaxCQ,
    NVMeWRRHigh,
    NVMeWRRMedium,
    NVMeMaxNamespace,
    NVMeDefaultNamespace,
    NVMeAttachDefaultNamespaces,
  };

  struct Disk {
    uint32_t nsid;
    bool enable;
    bool strict;
    bool useCoW;
    std::string path;
  };

  struct Namespace {
    Disk *pDisk;
    uint64_t capacity;
    uint32_t nsid;
    uint16_t lbaSize;
    uint16_t maxKeySize;
    uint32_t maxValueSize;
    uint32_t maxKeyCount;
    uint64_t zoneSize;
    uint32_t maxActiveZones;
    uint32_t maxOpenZones;
    uint8_t commandSet;
  };

 private:
  uint64_t workInterval;
  uint64_t requestQueueSize;

  PCIExpress::Generation pcieGen;
  uint8_t pcieLane;
  SATA::Generation sataGen;
  MIPI::M_PHY::Mode mphyMode;
  uint8_t mphyLane;

  uint16_t maxSQ;
  uint16_t maxCQ;
  uint16_t wrrHigh;
  uint16_t wrrMedium;
  uint32_t maxNamespace;
  uint32_t defaultNamespace;

  bool attachDefaultNamespaces;

  std::vector<Disk> diskList;
  std::vector<Namespace> namespaceList;

  void loadInterface(pugi::xml_node &) noexcept;
  void loadDisk(pugi::xml_node &, Disk *) noexcept;
  void loadNVMe(pugi::xml_node &) noexcept;
  void loadNamespace(pugi::xml_node &, Namespace *) noexcept;
  void storeInterface(pugi::xml_node &) noexcept;
  void storeDisk(pugi::xml_node &, Disk *) noexcept;
  void storeNVMe(pugi::xml_node &) noexcept;
  void storeNamespace(pugi::xml_node &, Namespace *) noexcept;

 public:
  Config();

  const char *getSectionName() noexcept override { return "hil"; }

  void loadFrom(pugi::xml_node &) noexcept override;
  void storeTo(pugi::xml_node &) noexcept override;
  void update() noexcept override;

  uint64_t readUint(uint32_t) const noexcept override;
  bool readBoolean(uint32_t) const noexcept override;
  bool writeUint(uint32_t, uint64_t) noexcept override;
  bool writeBoolean(uint32_t, bool) noexcept override;

  std::vector<Disk> &getDiskList() noexcept;
  std::vector<Namespace> &getNamespaceList() noexcept;
};

}  // namespace SimpleSSD::HIL

#endif
