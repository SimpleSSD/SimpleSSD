// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "sim/config_reader.hh"

#include <cstring>
#include <iostream>

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
void ConfigReader::load(const char *path) {
  auto result = file.load_file(path, pugi::parse_default, pugi::encoding_utf8);

  if (!result) {
    std::cerr << "Failed to parse configuration file: " << result.description()
              << std::endl;

    abort();
  }

  // Check node
  auto config = file.child(CONFIG_NODE_NAME);

  if (config) {
    // Travel sections
    for (auto section = config.first_child(); section;
         section = section.next_sibling()) {
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
    }

    // Update config objects
    simConfig.update();
    cpuConfig.update();
    memConfig.update();
  }

  // Close
  file.reset();
}

//! Load configuration from file
void ConfigReader::load(std::string &path) {
  load(path.c_str());
}

/**
 * \brief Save configuration to file
 *
 * \param[in] path Output file path
 */
void ConfigReader::save(const char *path) {
  // Create simplessd node
  auto config = file.append_child(CONFIG_NODE_NAME);
  config.append_attribute("version").set_value("2.1");  // TODO: FIX ME!

  // Append configuration sections
  pugi::xml_node section;

  STORE_SECTION(config, simConfig.getSectionName(), section);
  simConfig.storeTo(section);

  STORE_SECTION(config, cpuConfig.getSectionName(), section);
  cpuConfig.storeTo(section);

  STORE_SECTION(config, memConfig.getSectionName(), section)
  memConfig.storeTo(section);

  auto result =
      file.save_file(path, "  ", pugi::format_default, pugi::encoding_utf8);

  if (!result) {
    std::cerr << "Failed to save configuration file" << std::endl;

    abort();
  }
}

//! Save configuration to file
void ConfigReader::save(std::string &path) {
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
  }

  return ret;
}

}  // namespace SimpleSSD
