// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "sim/sim_config.hh"

#include <cstring>

namespace SimpleSSD {

const char NAME_OUTPUT_DIRECTORY[] = "OutputDirectory";
const char NAME_OUTPUT_FILE[] = "OutputFile";
const char NAME_ERROR_FILE[] = "ErrorFile";
const char NAME_DEBUG_FILE[] = "DebugFile";

//! A constructor
SimConfig::SimConfig() {}

//! A destructor
SimConfig::~SimConfig() {}

void SimConfig::loadFrom(pugi::xml_node &section) {
  for (auto node = section.first_child(); node; node = node.next_sibling()) {
    LOAD_NAME_TEXT(node, NAME_OUTPUT_DIRECTORY, outputDirectory, ".");
    LOAD_NAME_TEXT(node, NAME_OUTPUT_FILE, outputFile, "STDOUT");
    LOAD_NAME_TEXT(node, NAME_ERROR_FILE, errorFile, "STDERR");
    LOAD_NAME_TEXT(node, NAME_DEBUG_FILE, debugFile, "STDOUT");
  }
}

void SimConfig::storeTo(pugi::xml_node &section) {
  // Assume section node is empty
  STORE_NAME_TEXT(section, NAME_OUTPUT_DIRECTORY, outputDirectory.c_str());
  STORE_NAME_TEXT(section, NAME_OUTPUT_FILE, outputFile.c_str());
  STORE_NAME_TEXT(section, NAME_ERROR_FILE, errorFile.c_str());
  STORE_NAME_TEXT(section, NAME_DEBUG_FILE, debugFile.c_str());
}

std::string SimConfig::readString(uint32_t idx) {
  switch (idx) {
    case Key::OutputDirectory:
      return outputDirectory;
    case Key::OutputFile:
      return outputFile;
    case Key::ErrorFile:
      return errorFile;
    case Key::DebugFile:
      return debugFile;
  }

  return "";
}

bool SimConfig::writeString(uint32_t idx, std::string value) {
  bool ret = true;

  switch (idx) {
    case Key::OutputDirectory:
      outputDirectory = value;
      break;
    case Key::OutputFile:
      outputFile = value;
      break;
    case Key::ErrorFile:
      errorFile = value;
      break;
    case Key::DebugFile:
      debugFile = value;
      break;
    default:
      ret = false;
      break;
  }

  return ret;
}

}  // namespace SimpleSSD
