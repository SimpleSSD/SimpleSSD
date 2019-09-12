// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "sim/config.hh"

#include <cstring>
#include <iostream>

namespace SimpleSSD {

//! Config constructor
Config::Config() {}

//! Config destructor
Config::~Config() {}

/**
 * \brief Load configuration from file
 *
 * \param[in] path Input file path
 */
void Config::load(const char *path) {
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
    }

    // Update config objects
    simConfig.update();
  }

  // Close
  file.reset();
}

//! Load configuration from file
void Config::load(std::string &path) {
  load(path.c_str());
}

/**
 * \brief Save configuration to file
 *
 * \param[in] path Output file path
 */
void Config::save(const char *path) {
  // Create simplessd node
  auto config = file.append_child(CONFIG_NODE_NAME);
  config.append_attribute("version").set_value("2.1"); // TODO: FIX ME!

  // Append configuration sections
  pugi::xml_node section;

  STORE_SECTION(config, simConfig.getSectionName(), section);

  auto result =
      file.save_file(path, "  ", pugi::format_default, pugi::encoding_utf8);

  if (!result) {
    std::cerr << "Failed to save configuration file" << std::endl;

    abort();
  }
}

//! Save configuration to file
void Config::save(std::string &path) {
  save(path.c_str());
}

//! Read configuration as int64
int64_t Config::readInt(Section section, uint32_t key) {
  switch (section) {
    case Section::Sim:
      return simConfig.readInt(key);
  }

  return 0ll;
}

//! Read configuration as uint64
uint64_t Config::readUint(Section section, uint32_t key) {
  switch (section) {
    case Section::Sim:
      return simConfig.readUint(key);
  }

  return 0ull;
}

//! Read configuration as float
float Config::readFloat(Section section, uint32_t key) {
  switch (section) {
    case Section::Sim:
      return simConfig.readFloat(key);
  }

  return 0.f;
}

//! Read configuration as string
std::string Config::readString(Section section, uint32_t key) {
  switch (section) {
    case Section::Sim:
      return simConfig.readString(key);
  }

  return "";
}

//! Read configuration as boolean
bool Config::readBoolean(Section section, uint32_t key) {
  switch (section) {
    case Section::Sim:
      return simConfig.readBoolean(key);
  }

  return "";
}

//! Write configuration as int64
bool Config::writeInt(Section section, uint32_t key, int64_t value) {
  bool ret = false;

  switch (section) {
    case Section::Sim:
      ret = simConfig.writeInt(key, value);
      break;
  }

  return ret;
}

//! Write configuration as uint64
bool Config::writeUint(Section section, uint32_t key, uint64_t value) {
  bool ret = false;

  switch (section) {
    case Section::Sim:
      ret = simConfig.writeUint(key, value);
      break;
  }

  return ret;
}

//! Write configuration as float
bool Config::writeFloat(Section section, uint32_t key, float value) {
  bool ret = false;

  switch (section) {
    case Section::Sim:
      ret = simConfig.writeFloat(key, value);
      break;
  }

  return ret;
}

//! Write configuration as string
bool Config::writeString(Section section, uint32_t key, std::string value) {
  bool ret = false;

  switch (section) {
    case Section::Sim:
      ret = simConfig.writeString(key, value);
      break;
  }

  return ret;
}

//! Write configuration as boolean
bool Config::writeBoolean(Section section, uint32_t key, bool value) {
  bool ret = false;

  switch (section) {
    case Section::Sim:
      ret = simConfig.writeBoolean(key, value);
      break;
  }

  return ret;
}

}  // namespace SimpleSSD
