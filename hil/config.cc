// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "hil/config.hh"

#include <filesystem>

#include "util/algorithm.hh"

namespace SimpleSSD::HIL {

const char NAME_WORK_INTERVAL[] = "WorkInterval";
const char NAME_FIFO_SIZE[] = "RequestQueueSize";
const char NAME_GENERATION[] = "Generation";
const char NAME_LANE[] = "Lane";
const char NAME_MODE[] = "Mode";
const char NAME_WIDTH[] = "Width";
const char NAME_CLOCK[] = "Clock";
const char NAME_ENABLE_DISK_IMAGE[] = "EnableDiskImage";
const char NAME_STRICT_SIZE_CHECK[] = "StrickSizecheck";
const char NAME_USE_COW_DISK[] = "UseCopyOnWriteDisk";
const char NAME_DISK_IMAGE_PATH[] = "DiskImagePath";
const char NAME_MAX_SQ[] = "MaxSQ";
const char NAME_MAX_CQ[] = "MaxCQ";
const char NAME_WRR_HIGH[] = "WRRHigh";
const char NAME_WRR_MEDIUM[] = "WRRMedium";
const char NAME_MAX_NAMESPACE[] = "MaxNamespace";
const char NAME_DEFAULT_NAMESPACE[] = "DefaultNamespace";
const char NAME_ATTACH_DEFAULT_NAMESPACES[] = "AttachDefaultNamespaces";
const char NAME_LBA_SIZE[] = "LBASize";
const char NAME_CAPACITY[] = "Capacity";

//! A constructor
Config::Config() {
  workInterval = 1000000;
  requestQueueSize = 8;

  pcieGen = PCIExpress::Generation::Gen3;
  pcieLane = 4;
  sataGen = SATA::Generation::Gen3;
  mphyMode = MIPI::M_PHY::Mode::HighSpeed_Gear3;
  mphyLane = 2;
  axiWidth = ARM::AXI::Width::Bit256;
  axiClock = 100000000;

  maxSQ = 16;
  maxCQ = 16;
  wrrHigh = 2;
  wrrMedium = 2;
  maxNamespace = 16;
  defaultNamespace = 0;
}

//! A destructor
Config::~Config() {}

void Config::loadInterface(pugi::xml_node &section) {
  for (auto node = section.first_child(); node; node = node.next_sibling()) {
    auto name = node.attribute("name").value();

    if (strcmp(name, "pcie") == 0 && isSection(node)) {
      for (auto node2 = node.first_child(); node2;
           node2 = node2.next_sibling()) {
        LOAD_NAME_UINT_TYPE(node2, NAME_GENERATION, PCIExpress::Generation,
                            pcieGen);
        LOAD_NAME_UINT_TYPE(node2, NAME_LANE, uint8_t, pcieLane);
      }
    }
    else if (strcmp(name, "sata") == 0 && isSection(node)) {
      for (auto node2 = node.first_child(); node2;
           node2 = node2.next_sibling()) {
        LOAD_NAME_UINT_TYPE(node2, NAME_GENERATION, SATA::Generation, sataGen);
      }
    }
    else if (strcmp(name, "mphy") == 0 && isSection(node)) {
      for (auto node2 = node.first_child(); node2;
           node2 = node2.next_sibling()) {
        LOAD_NAME_UINT_TYPE(node2, NAME_MODE, MIPI::M_PHY::Mode, mphyMode);
        LOAD_NAME_UINT_TYPE(node2, NAME_LANE, uint8_t, mphyLane);
      }
    }
    else if (strcmp(name, "axi") == 0 && isSection(node)) {
      for (auto node2 = node.first_child(); node2;
           node2 = node2.next_sibling()) {
        LOAD_NAME_UINT_TYPE(node2, NAME_WIDTH, ARM::AXI::Width, axiWidth);
        LOAD_NAME_UINT(node2, NAME_CLOCK, axiClock);
      }
    }
  }
}

void Config::loadDisk(pugi::xml_node &section, Disk *disk) {
  disk->nsid = strtoul(section.attribute("nsid").value(), nullptr, 10);

  for (auto node = section.first_child(); node; node = node.next_sibling()) {
    LOAD_NAME_BOOLEAN(node, NAME_ENABLE_DISK_IMAGE, disk->enable);
    LOAD_NAME_BOOLEAN(node, NAME_STRICT_SIZE_CHECK, disk->strict);
    LOAD_NAME_BOOLEAN(node, NAME_USE_COW_DISK, disk->useCoW);
    LOAD_NAME_STRING(node, NAME_DISK_IMAGE_PATH, disk->path);
  }
}

void Config::loadNVMe(pugi::xml_node &section) {
  for (auto node = section.first_child(); node; node = node.next_sibling()) {
    LOAD_NAME_UINT_TYPE(node, NAME_MAX_SQ, uint16_t, maxSQ);
    LOAD_NAME_UINT_TYPE(node, NAME_MAX_CQ, uint16_t, maxCQ);
    LOAD_NAME_UINT_TYPE(node, NAME_WRR_HIGH, uint16_t, wrrHigh);
    LOAD_NAME_UINT_TYPE(node, NAME_WRR_MEDIUM, uint16_t, wrrMedium);
    LOAD_NAME_UINT_TYPE(node, NAME_MAX_NAMESPACE, uint32_t, maxNamespace);
    LOAD_NAME_UINT_TYPE(node, NAME_DEFAULT_NAMESPACE, uint32_t,
                        defaultNamespace);
    LOAD_NAME_BOOLEAN(node, NAME_ATTACH_DEFAULT_NAMESPACES,
                      attachDefaultNamespaces);

    if (strcmp(node.attribute("name").value(), "namespace") == 0 &&
        isSection(node)) {
      namespaceList.push_back(Namespace());

      loadNamespace(node, &namespaceList.back());
    }
  }
}

void Config::loadNamespace(pugi::xml_node &section, Namespace *ns) {
  ns->nsid = strtoul(section.attribute("nsid").value(), nullptr, 10);

  for (auto node = section.first_child(); node; node = node.next_sibling()) {
    LOAD_NAME_UINT_TYPE(node, NAME_LBA_SIZE, uint16_t, ns->lbaSize);
    LOAD_NAME_UINT(node, NAME_CAPACITY, ns->capacity);
  }
}

void Config::storeInterface(pugi::xml_node &section) {
  pugi::xml_node node;

  STORE_SECTION(section, "pcie", node);
  STORE_NAME_UINT(node, NAME_GENERATION, pcieGen);
  STORE_NAME_UINT(node, NAME_LANE, pcieLane);

  STORE_SECTION(section, "sata", node);
  STORE_NAME_UINT(node, NAME_GENERATION, sataGen);

  STORE_SECTION(section, "mphy", node);
  STORE_NAME_UINT(node, NAME_MODE, mphyMode);
  STORE_NAME_UINT(node, NAME_LANE, mphyLane);

  STORE_SECTION(section, "axi", node);
  STORE_NAME_UINT(node, NAME_WIDTH, axiWidth * 8);  // Byte -> Bit
  STORE_NAME_UINT(node, NAME_CLOCK, axiClock);
}

void Config::storeDisk(pugi::xml_node &section, Disk *disk) {
  section.append_attribute("nsid").set_value(disk->nsid);

  STORE_NAME_BOOLEAN(section, NAME_ENABLE_DISK_IMAGE, disk->enable);
  STORE_NAME_BOOLEAN(section, NAME_STRICT_SIZE_CHECK, disk->strict);
  STORE_NAME_BOOLEAN(section, NAME_USE_COW_DISK, disk->useCoW);
  STORE_NAME_STRING(section, NAME_DISK_IMAGE_PATH, disk->path);
}

void Config::storeNVMe(pugi::xml_node &section) {
  pugi::xml_node node;

  STORE_NAME_UINT(section, NAME_MAX_SQ, maxSQ);
  STORE_NAME_UINT(section, NAME_MAX_CQ, maxCQ);
  STORE_NAME_UINT(section, NAME_WRR_HIGH, wrrHigh);
  STORE_NAME_UINT(section, NAME_WRR_MEDIUM, wrrMedium);
  STORE_NAME_UINT(section, NAME_MAX_NAMESPACE, maxNamespace);
  STORE_NAME_UINT(section, NAME_DEFAULT_NAMESPACE, defaultNamespace);
  STORE_NAME_BOOLEAN(section, NAME_ATTACH_DEFAULT_NAMESPACES,
                     attachDefaultNamespaces);

  for (auto &ns : namespaceList) {
    STORE_SECTION(section, "namespace", node);

    storeNamespace(node, &ns);
  }
}

void Config::storeNamespace(pugi::xml_node &section, Namespace *ns) {
  section.attribute("nsid").set_value(ns->nsid);

  STORE_NAME_UINT(section, NAME_LBA_SIZE, ns->lbaSize);
  STORE_NAME_UINT(section, NAME_CAPACITY, ns->capacity);
}

void Config::loadFrom(pugi::xml_node &section) {
  for (auto node = section.first_child(); node; node = node.next_sibling()) {
    auto name = node.attribute("name").value();

    LOAD_NAME_TIME(section, NAME_WORK_INTERVAL, workInterval);
    LOAD_NAME_UINT(section, NAME_FIFO_SIZE, requestQueueSize);

    if (strcmp(name, "interface") == 0 && isSection(node)) {
      loadInterface(node);
    }
    else if (strcmp(name, "disk") == 0 && isSection(node)) {
      diskList.push_back(Disk());

      loadDisk(node, &diskList.back());
    }
    else if (strcmp(name, "nvme") == 0 && isSection(node)) {
      loadNVMe(node);
    }
  }
}

void Config::storeTo(pugi::xml_node &section) {
  pugi::xml_node node;

  STORE_NAME_UINT(section, NAME_WORK_INTERVAL, workInterval);
  STORE_NAME_UINT(section, NAME_FIFO_SIZE, requestQueueSize);

  STORE_SECTION(section, "interface", node);
  storeInterface(node);

  for (auto &disk : diskList) {
    STORE_SECTION(section, "disk", node);

    storeDisk(node, &disk);
  }

  STORE_SECTION(section, "nvme", node);
  storeNVMe(node);
}

void Config::update() {
  // Validates
  warn_if(workInterval >= 1000000000, "Work interval %" PRIu64 " is too large.",
          workInterval);
  panic_if(requestQueueSize == 0, "Invalid request queue size.");

  pcieGen = (PCIExpress::Generation)((uint8_t)pcieGen - 1);
  panic_if((uint8_t)pcieGen > 2, "Invalid PCIe generation %u.",
           (uint8_t)pcieGen + 1);
  panic_if(popcount16(pcieLane) != 1 || pcieLane > 32, "Invalid PCIe lane %u.",
           pcieLane);

  sataGen = (SATA::Generation)((uint8_t)sataGen - 1);
  panic_if((uint8_t)sataGen > 2, "Invalid SATA generation %u.",
           (uint8_t)sataGen + 1);

  panic_if((uint8_t)mphyMode > 3, "Invalid M-PHY mode %u.", (uint8_t)mphyMode);
  panic_if(mphyLane == 0 || mphyLane > 2, "Invalid M-PHY lane %u.", mphyLane);

  panic_if(popcount16((uint16_t)axiWidth) != 1 || (uint16_t)axiWidth > 1024 ||
               (uint16_t)axiWidth < 32,
           "Invalid AXI bus width %u.", (uint16_t)axiWidth);
  axiWidth = (ARM::AXI::Width)((uint16_t)axiWidth / 8);

  for (auto &disk : diskList) {
    panic_if(
        disk.nsid == 0 || disk.nsid == 0xFFFFFFFF || disk.nsid > maxNamespace,
        "Invalid namespace ID %u in disk config.", disk.nsid);

    if (disk.enable) {
      panic_if(!std::filesystem::exists(disk.path),
               "Specified disk image %s does not exists.", disk.path.c_str());
    }
  }

  panic_if(maxSQ < 2, "NVMe requires at least two submission queues.");
  panic_if(maxCQ < 2, "NVMe requires at least two completion queues.");
  panic_if(wrrHigh == 0 || wrrHigh > 256,
           "Invalid weighted-round-robin high priority value %u.", wrrHigh);
  panic_if(wrrMedium == 0 || wrrMedium > 256,
           "Invalid weighted-round-robin medium priority value %u.", wrrMedium);
  panic_if(maxNamespace == 0, "Invalid maximum namespace value %u.",
           maxNamespace);
  panic_if(defaultNamespace > maxNamespace,
           "Too many default namespaces (%u > %u).", defaultNamespace,
           maxNamespace);

  for (auto &ns : namespaceList) {
    panic_if(ns.nsid == 0 || ns.nsid == 0xFFFFFFFF || ns.nsid > maxNamespace,
             "Invalid namespace ID %u in namespace config.", ns.nsid);

    panic_if(popcount16(ns.lbaSize) != 1 || ns.lbaSize < 512,
             "Invalid logical block size %u.", ns.lbaSize);
  }

  // Make link between disk and namespace
  std::sort(
      diskList.begin(), diskList.end(),
      [](const Disk &a, const Disk &b) -> bool {
        panic_if(UNLIKELY(a.nsid == b.nsid),
                 "Duplicated namespace ID %u while sorting disk configuration.",
                 a.nsid);

        return a.nsid < b.nsid;
      });
  std::sort(
      namespaceList.begin(), namespaceList.end(),
      [](const Namespace &a, const Namespace &b) -> bool {
        panic_if(
            UNLIKELY(a.nsid == b.nsid),
            "Duplicated namespace ID %u while sorting namespace configuration.",
            a.nsid);

        return a.nsid < b.nsid;
      });

  // Simple O(n^2) algorithm
  // Do we need to change this to O(n) algorithm?
  for (auto &ns : namespaceList) {
    ns.pDisk = nullptr;

    panic_if(ns.nsid > maxNamespace,
             "Namespace ID is greater than MaxNamespace.");
    panic_if((ns.capacity % ns.lbaSize) != 0,
             "Invalid capacity - not aligned to LBASize");

    for (auto &disk : diskList) {
      if (ns.nsid == disk.nsid) {
        ns.pDisk = &disk;

        break;
      }
    }
  }
}

uint64_t Config::readUint(uint32_t idx) {
  uint64_t ret = 0;

  switch (idx) {
    case Key::WorkInterval:
      ret = workInterval;
      break;
    case Key::RequestQueueSize:
      ret = requestQueueSize;
      break;
    case Key::PCIeGeneration:
      ret = (uint64_t)pcieGen;
      break;
    case Key::PCIeLane:
      ret = pcieLane;
      break;
    case Key::SATAGeneration:
      ret = (uint64_t)sataGen;
      break;
    case Key::MPHYMode:
      ret = (uint64_t)mphyMode;
      break;
    case Key::MPHYLane:
      ret = mphyLane;
      break;
    case Key::AXIWidth:
      ret = (uint64_t)axiWidth;
      break;
    case Key::AXIClock:
      ret = axiClock;
      break;
    case Key::NVMeMaxSQ:
      ret = maxSQ;
      break;
    case Key::NVMeMaxCQ:
      ret = maxCQ;
      break;
    case Key::NVMeWRRHigh:
      ret = wrrHigh;
      break;
    case Key::NVMeWRRMedium:
      ret = wrrMedium;
      break;
    case Key::NVMeMaxNamespace:
      ret = maxNamespace;
      break;
    case Key::NVMeDefaultNamespace:
      ret = defaultNamespace;
      break;
  }

  return ret;
}

bool Config::readBoolean(uint32_t idx) {
  switch (idx) {
    case Key::NVMeAttachDefaultNamespaces:
      return attachDefaultNamespaces;
  }

  return false;
}

bool Config::writeUint(uint32_t idx, uint64_t value) {
  bool ret = true;

  switch (idx) {
    case Key::WorkInterval:
      workInterval = value;
      break;
    case Key::RequestQueueSize:
      requestQueueSize = value;
      break;
    case Key::PCIeGeneration:
      pcieGen = (PCIExpress::Generation)value;
      break;
    case Key::PCIeLane:
      pcieLane = value;
      break;
    case Key::SATAGeneration:
      sataGen = (SATA::Generation)value;
      break;
    case Key::MPHYMode:
      mphyMode = (MIPI::M_PHY::Mode)value;
      break;
    case Key::MPHYLane:
      mphyLane = value;
      break;
    case Key::AXIWidth:
      axiWidth = (ARM::AXI::Width)value;
      break;
    case Key::AXIClock:
      axiClock = value;
      break;
    case Key::NVMeMaxSQ:
      maxSQ = value;
      break;
    case Key::NVMeMaxCQ:
      maxCQ = value;
      break;
    case Key::NVMeWRRHigh:
      wrrHigh = value;
      break;
    case Key::NVMeWRRMedium:
      wrrMedium = value;
      break;
    case Key::NVMeMaxNamespace:
      maxNamespace = value;
      break;
    case Key::NVMeDefaultNamespace:
      defaultNamespace = value;
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
    case Key::NVMeAttachDefaultNamespaces:
      attachDefaultNamespaces = value;
      break;
    default:
      ret = false;
      break;
  }

  return ret;
}

std::vector<Config::Disk> &Config::getDiskList() {
  return diskList;
}

std::vector<Config::Namespace> &Config::getNamespaceList() {
  return namespaceList;
}

}  // namespace SimpleSSD::HIL
