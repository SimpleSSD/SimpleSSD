// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "sim/config_reader.hh"

#include <cstring>
#include <iostream>

#include "sim/version.hh"

namespace SimpleSSD {

//! ConfigReader constructor
ConfigReader::ConfigReader() {}

//! ConfigReader destructor
ConfigReader::~ConfigReader() {}

/**
 * \brief Load configuration from file
 *
 * \param[in] path Input file path
 */
void ConfigReader::load(const char *path) noexcept {
  auto result = file.load_file(
      path, pugi::parse_default | pugi::parse_trim_pcdata, pugi::encoding_utf8);

  if (!result) {
    std::cerr << "Failed to parse configuration file: " << result.description()
              << std::endl;

    abort();
  }

  // Check node
  auto config = file.child(CONFIG_NODE_NAME);

  if (config) {
    // Check version
    auto version = config.attribute("version").value();
    if (strncmp(version, SIMPLESSD_VERSION, strlen(SIMPLESSD_VERSION)) != 0) {
      std::cerr << "SimpleSSD configuration file version is different." << std::endl;
      std::cerr << " File version: " << version << std::endl;
      std::cerr << " Program version: " << SIMPLESSD_VERSION << std::endl;
    }

    // Travel sections
    for (auto section = config.first_child(); section;
         section = section.next_sibling()) {
      if (strcmp(section.name(), CONFIG_SECTION_NAME)) {
        continue;
      }

      auto name = section.attribute(CONFIG_ATTRIBUTE).value();

      if (strcmp(name, simConfig.getSectionName()) == 0) {
        simConfig.loadFrom(section);
      }
      else if (strcmp(name, cpuConfig.getSectionName()) == 0) {
        cpuConfig.loadFrom(section);
      }
      else if (strcmp(name, memConfig.getSectionName()) == 0) {
        memConfig.loadFrom(section);
      }
      else if (strcmp(name, hilConfig.getSectionName()) == 0) {
        hilConfig.loadFrom(section);
      }
      else if (strcmp(name, iclConfig.getSectionName()) == 0) {
        iclConfig.loadFrom(section);
      }
      else if (strcmp(name, ftlConfig.getSectionName()) == 0) {
        ftlConfig.loadFrom(section);
      }
      else if (strcmp(name, filConfig.getSectionName()) == 0) {
        filConfig.loadFrom(section);
      }
    }

    // Update config objects
    simConfig.update();
    cpuConfig.update();
    memConfig.update();
    hilConfig.update();
    iclConfig.update();
    ftlConfig.update();
    filConfig.update();
  }

  // Close
  file.reset();
}

//! Load configuration from file
void ConfigReader::load(std::string &path) noexcept {
  load(path.c_str());
}

/**
 * \brief Save configuration to file
 *
 * \param[in] path Output file path
 */
void ConfigReader::save(const char *path) noexcept {
  // Create simplessd node
  auto config = file.append_child(CONFIG_NODE_NAME);
  config.append_attribute("version").set_value(SIMPLESSD_VERSION);

  // Append configuration sections
  pugi::xml_node section;

  STORE_SECTION(config, simConfig.getSectionName(), section);
  simConfig.storeTo(section);

  STORE_SECTION(config, cpuConfig.getSectionName(), section);
  cpuConfig.storeTo(section);

  STORE_SECTION(config, memConfig.getSectionName(), section)
  memConfig.storeTo(section);

  STORE_SECTION(config, hilConfig.getSectionName(), section)
  hilConfig.storeTo(section);

  STORE_SECTION(config, iclConfig.getSectionName(), section)
  iclConfig.storeTo(section);

  STORE_SECTION(config, ftlConfig.getSectionName(), section)
  ftlConfig.storeTo(section);

  STORE_SECTION(config, filConfig.getSectionName(), section)
  filConfig.storeTo(section);

  auto result =
      file.save_file(path, "  ", pugi::format_default, pugi::encoding_utf8);

  if (!result) {
    std::cerr << "Failed to save configuration file" << std::endl;

    abort();
  }
}

//! Save configuration to file
void ConfigReader::save(std::string &path) noexcept {
  save(path.c_str());
}

//! Read configuration as int64
int64_t ConfigReader::readInt(Section section, uint32_t key) {
  switch (section) {
    case Section::Simulation:
      return simConfig.readInt(key);
    case Section::CPU:
      return cpuConfig.readInt(key);
    case Section::Memory:
      return memConfig.readInt(key);
    case Section::HostInterface:
      return hilConfig.readInt(key);
    case Section::InternalCache:
      return iclConfig.readInt(key);
    case Section::FlashTranslation:
      return ftlConfig.readInt(key);
    case Section::FlashInterface:
      return filConfig.readInt(key);
  }

  return 0ll;
}

//! Read configuration as uint64
uint64_t ConfigReader::readUint(Section section, uint32_t key) {
  switch (section) {
    case Section::Simulation:
      return simConfig.readUint(key);
    case Section::CPU:
      return cpuConfig.readUint(key);
    case Section::Memory:
      return memConfig.readUint(key);
    case Section::HostInterface:
      return hilConfig.readUint(key);
    case Section::InternalCache:
      return iclConfig.readUint(key);
    case Section::FlashTranslation:
      return ftlConfig.readUint(key);
    case Section::FlashInterface:
      return filConfig.readUint(key);
  }

  return 0ull;
}

//! Read configuration as float
float ConfigReader::readFloat(Section section, uint32_t key) {
  switch (section) {
    case Section::Simulation:
      return simConfig.readFloat(key);
    case Section::CPU:
      return cpuConfig.readFloat(key);
    case Section::Memory:
      return memConfig.readFloat(key);
    case Section::HostInterface:
      return hilConfig.readFloat(key);
    case Section::InternalCache:
      return iclConfig.readFloat(key);
    case Section::FlashTranslation:
      return ftlConfig.readFloat(key);
    case Section::FlashInterface:
      return filConfig.readFloat(key);
  }

  return 0.f;
}

//! Read configuration as string
std::string ConfigReader::readString(Section section, uint32_t key) {
  switch (section) {
    case Section::Simulation:
      return simConfig.readString(key);
    case Section::CPU:
      return cpuConfig.readString(key);
    case Section::Memory:
      return memConfig.readString(key);
    case Section::HostInterface:
      return hilConfig.readString(key);
    case Section::InternalCache:
      return iclConfig.readString(key);
    case Section::FlashTranslation:
      return ftlConfig.readString(key);
    case Section::FlashInterface:
      return filConfig.readString(key);
  }

  return "";
}

//! Read configuration as boolean
bool ConfigReader::readBoolean(Section section, uint32_t key) {
  switch (section) {
    case Section::Simulation:
      return simConfig.readBoolean(key);
    case Section::CPU:
      return cpuConfig.readBoolean(key);
    case Section::Memory:
      return memConfig.readBoolean(key);
    case Section::HostInterface:
      return hilConfig.readBoolean(key);
    case Section::InternalCache:
      return iclConfig.readBoolean(key);
    case Section::FlashTranslation:
      return ftlConfig.readBoolean(key);
    case Section::FlashInterface:
      return filConfig.readBoolean(key);
  }

  return "";
}

//! Write configuration as int64
bool ConfigReader::writeInt(Section section, uint32_t key, int64_t value) {
  bool ret = false;

  switch (section) {
    case Section::Simulation:
      ret = simConfig.writeInt(key, value);
      break;
    case Section::CPU:
      ret = cpuConfig.writeInt(key, value);
      break;
    case Section::Memory:
      ret = memConfig.writeInt(key, value);
      break;
    case Section::HostInterface:
      ret = hilConfig.writeInt(key, value);
      break;
    case Section::InternalCache:
      ret = iclConfig.writeInt(key, value);
      break;
    case Section::FlashTranslation:
      ret = ftlConfig.writeInt(key, value);
      break;
    case Section::FlashInterface:
      ret = filConfig.writeInt(key, value);
      break;
  }

  return ret;
}

//! Write configuration as uint64
bool ConfigReader::writeUint(Section section, uint32_t key, uint64_t value) {
  bool ret = false;

  switch (section) {
    case Section::Simulation:
      ret = simConfig.writeUint(key, value);
      break;
    case Section::CPU:
      ret = cpuConfig.writeUint(key, value);
      break;
    case Section::Memory:
      ret = memConfig.writeUint(key, value);
      break;
    case Section::HostInterface:
      ret = hilConfig.writeUint(key, value);
      break;
    case Section::InternalCache:
      ret = iclConfig.writeUint(key, value);
      break;
    case Section::FlashTranslation:
      ret = ftlConfig.writeUint(key, value);
      break;
    case Section::FlashInterface:
      ret = filConfig.writeUint(key, value);
      break;
  }

  return ret;
}

//! Write configuration as float
bool ConfigReader::writeFloat(Section section, uint32_t key, float value) {
  bool ret = false;

  switch (section) {
    case Section::Simulation:
      ret = simConfig.writeFloat(key, value);
      break;
    case Section::CPU:
      ret = cpuConfig.writeFloat(key, value);
      break;
    case Section::Memory:
      ret = memConfig.writeFloat(key, value);
      break;
    case Section::HostInterface:
      ret = hilConfig.writeFloat(key, value);
      break;
    case Section::InternalCache:
      ret = iclConfig.writeFloat(key, value);
      break;
    case Section::FlashTranslation:
      ret = ftlConfig.writeFloat(key, value);
      break;
    case Section::FlashInterface:
      ret = filConfig.writeFloat(key, value);
      break;
  }

  return ret;
}

//! Write configuration as string
bool ConfigReader::writeString(Section section, uint32_t key,
                               std::string value) {
  bool ret = false;

  switch (section) {
    case Section::Simulation:
      ret = simConfig.writeString(key, value);
      break;
    case Section::CPU:
      ret = cpuConfig.writeString(key, value);
      break;
    case Section::Memory:
      ret = memConfig.writeString(key, value);
      break;
    case Section::HostInterface:
      ret = hilConfig.writeString(key, value);
      break;
    case Section::InternalCache:
      ret = iclConfig.writeString(key, value);
      break;
    case Section::FlashTranslation:
      ret = ftlConfig.writeString(key, value);
      break;
    case Section::FlashInterface:
      ret = filConfig.writeString(key, value);
      break;
  }

  return ret;
}

//! Write configuration as boolean
bool ConfigReader::writeBoolean(Section section, uint32_t key, bool value) {
  bool ret = false;

  switch (section) {
    case Section::Simulation:
      ret = simConfig.writeBoolean(key, value);
      break;
    case Section::CPU:
      ret = cpuConfig.writeBoolean(key, value);
      break;
    case Section::Memory:
      ret = memConfig.writeBoolean(key, value);
      break;
    case Section::HostInterface:
      ret = hilConfig.writeBoolean(key, value);
      break;
    case Section::InternalCache:
      ret = iclConfig.writeBoolean(key, value);
      break;
    case Section::FlashTranslation:
      ret = ftlConfig.writeBoolean(key, value);
      break;
    case Section::FlashInterface:
      ret = filConfig.writeBoolean(key, value);
      break;
  }

  return ret;
}

Memory::Config::SRAMStructure *ConfigReader::getSRAM() {
  return memConfig.getSRAM();
}

Memory::Config::DRAMStructure *ConfigReader::getDRAM() {
  return memConfig.getDRAM();
}

Memory::Config::DRAMTiming *ConfigReader::getDRAMTiming() {
  return memConfig.getDRAMTiming();
}

Memory::Config::DRAMPower *ConfigReader::getDRAMPower() {
  return memConfig.getDRAMPower();
}

Memory::Config::TimingDRAMConfig *ConfigReader::getTimingDRAM() {
  return memConfig.getTimingDRAM();
}

std::vector<HIL::Config::Disk> &ConfigReader::getDiskList() {
  return hilConfig.getDiskList();
}

std::vector<HIL::Config::Namespace> &ConfigReader::getNamespaceList() {
  return hilConfig.getNamespaceList();
}

FIL::Config::NANDStructure *ConfigReader::getNANDStructure() {
  return filConfig.getNANDStructure();
}
FIL::Config::NANDTiming *ConfigReader::getNANDTiming() {
  return filConfig.getNANDTiming();
}
FIL::Config::NANDPower *ConfigReader::getNANDPower() {
  return filConfig.getNANDPower();
}

}  // namespace SimpleSSD
