/*
 * Copyright (C) 2017 CAMELab
 *
 * This file is part of SimpleSSD.
 *
 * SimpleSSD is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * SimpleSSD is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with SimpleSSD.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "util/config.hh"

#include "log/trace.hh"

#ifdef _MSC_VER
#define strcasecmp _stricmp
#else
#include <strings.h>
#endif

namespace SimpleSSD {

const char SECTION_NVME[] = "nvme";
const char SECTION_FTL[] = "ftl";
const char SECTION_ICL[] = "icl";
const char SECTION_PAL[] = "pal";
const char SECTION_DRAM[] = "dram";

bool BaseConfig::convertBool(const char *value) {
  bool ret = false;

  if (strcasecmp(value, "true") == 0 || strtoul(value, nullptr, 10)) {
    ret = true;
  }

  return ret;
}

bool ConfigReader::init(std::string file) {
  if (ini_parse(file.c_str(), parserHandler, this) < 0) {
    return false;
  }

  // Update all
  nvmeConfig.update();
  ftlConfig.update();
  iclConfig.update();
  palConfig.update();
  dramConfig.update();

  return true;
}

int ConfigReader::parserHandler(void *context, const char *section,
                                const char *name, const char *value) {
  ConfigReader *pThis = (ConfigReader *)context;
  bool handled = false;

  if (MATCH_SECTION(SECTION_NVME)) {
    handled = pThis->nvmeConfig.setConfig(name, value);
  }
  else if (MATCH_SECTION(SECTION_FTL)) {
    handled = pThis->ftlConfig.setConfig(name, value);
  }
  else if (MATCH_SECTION(SECTION_ICL)) {
    handled = pThis->iclConfig.setConfig(name, value);
  }
  else if (MATCH_SECTION(SECTION_PAL)) {
    handled = pThis->palConfig.setConfig(name, value);
  }
  else if (MATCH_SECTION(SECTION_DRAM)) {
    handled = pThis->dramConfig.setConfig(name, value);
  }

  if (!handled) {
    Logger::warn("Config [%s] %s = %s not handled", section, name, value);
  }

  return 1;
}

}  // namespace SimpleSSD
