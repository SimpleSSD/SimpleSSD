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
    AXIWidth,
    AXIClock,
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
    uint32_t nsid;
    uint16_t lbaSize;
    uint64_t capacity;
    Disk *pDisk;
  };

 private:
  uint64_t workInterval;
  uint64_t requestQueueSize;

  PCIExpress::Generation pcieGen;
  uint8_t pcieLane;
  SATA::Generation sataGen;
  MIPI::M_PHY::Mode mphyMode;
  uint8_t mphyLane;
  ARM::AXI::Width axiWidth;
  uint64_t axiClock;

  uint16_t maxSQ;
  uint16_t maxCQ;
  uint16_t wrrHigh;
  uint16_t wrrMedium;
  uint32_t maxNamespace;
  uint32_t defaultNamespace;
  bool attachDefaultNamespaces;

  std::vector<Disk> diskList;
  std::vector<Namespace> namespaceList;

  void loadInterface(pugi::xml_node &);
  void loadDisk(pugi::xml_node &, Disk *);
  void loadNVMe(pugi::xml_node &);
  void loadNamespace(pugi::xml_node &, Namespace *);
  void storeInterface(pugi::xml_node &);
  void storeDisk(pugi::xml_node &, Disk *);
  void storeNVMe(pugi::xml_node &);
  void storeNamespace(pugi::xml_node &, Namespace *);

 public:
  Config();
  ~Config();

  const char *getSectionName() override { return "hil"; }

  void loadFrom(pugi::xml_node &) override;
  void storeTo(pugi::xml_node &) override;
  void update() override;

  uint64_t readUint(uint32_t) override;
  bool readBoolean(uint32_t) override;
  bool writeUint(uint32_t, uint64_t) override;
  bool writeBoolean(uint32_t, bool) override;

  std::vector<Disk> &getDiskList();
  std::vector<Namespace> &getNamespaceList();
};

}  // namespace SimpleSSD::HIL

#endif
