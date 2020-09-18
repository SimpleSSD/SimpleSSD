// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "sim/config.hh"

#include <cstring>

namespace SimpleSSD {

const char NAME_OUTPUT_DIRECTORY[] = "OutputDirectory";
const char NAME_OUTPUT_FILE[] = "OutputFile";
const char NAME_ERROR_FILE[] = "ErrorFile";
const char NAME_DEBUG_FILE[] = "DebugFile";
const char NAME_CONTROLLER[] = "Controller";
const char NAME_RESTORE[] = "RestoreFromCheckpoint";

//! A constructor
Config::Config() {
  outputDirectory = ".";
  outputFile = FILE_STDOUT;
  errorFile = FILE_STDERR;
  debugFile = FILE_STDOUT;
  mode = Mode::None;
  restore = false;
}

void Config::loadFrom(pugi::xml_node &section) noexcept {
  for (auto node = section.first_child(); node; node = node.next_sibling()) {
    LOAD_NAME_STRING(node, NAME_OUTPUT_DIRECTORY, outputDirectory);
    LOAD_NAME_STRING(node, NAME_OUTPUT_FILE, outputFile);
    LOAD_NAME_STRING(node, NAME_ERROR_FILE, errorFile);
    LOAD_NAME_STRING(node, NAME_DEBUG_FILE, debugFile);
    LOAD_NAME_UINT_TYPE(node, NAME_CONTROLLER, Mode, mode);
  }
}

void Config::storeTo(pugi::xml_node &section) noexcept {
  // Assume section node is empty
  STORE_NAME_STRING(section, NAME_OUTPUT_DIRECTORY, outputDirectory);
  STORE_NAME_STRING(section, NAME_OUTPUT_FILE, outputFile);
  STORE_NAME_STRING(section, NAME_ERROR_FILE, errorFile);
  STORE_NAME_STRING(section, NAME_DEBUG_FILE, debugFile);
  STORE_NAME_UINT(section, NAME_CONTROLLER, mode);
}

uint64_t Config::readUint(uint32_t idx) const noexcept {
  switch (idx) {
    case Controller:
      return (uint64_t)mode;
  }

  return 0;
}

std::string Config::readString(uint32_t idx) const noexcept {
  switch (idx) {
    case OutputDirectory:
      return outputDirectory;
    case OutputFile:
      return outputFile;
    case ErrorFile:
      return errorFile;
    case DebugFile:
      return debugFile;
  }

  return "";
}

bool Config::readBoolean(uint32_t idx) const noexcept {
  switch (idx) {
    case RestoreFromCheckpoint:
      return restore;
  }

  return false;
}

bool Config::writeUint(uint32_t idx, uint64_t value) noexcept {
  bool ret = true;

  switch (idx) {
    case Controller:
      mode = (Mode)value;
      break;
    default:
      ret = false;
      break;
  }

  return ret;
}

bool Config::writeString(uint32_t idx, std::string &value) noexcept {
  bool ret = true;

  switch (idx) {
    case OutputDirectory:
      outputDirectory = value;
      break;
    case OutputFile:
      outputFile = value;
      break;
    case ErrorFile:
      errorFile = value;
      break;
    case DebugFile:
      debugFile = value;
      break;
    default:
      ret = false;
      break;
  }

  return ret;
}

bool Config::writeBoolean(uint32_t idx, bool value) noexcept {
  bool ret = true;

  switch (idx) {
    case RestoreFromCheckpoint:
      restore = value;
      break;
    default:
      ret = false;
      break;
  }

  return ret;
}

}  // namespace SimpleSSD
