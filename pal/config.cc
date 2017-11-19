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

#include "pal/config.hh"

#include "log/trace.hh"

namespace SimpleSSD {

namespace PAL {

const char NAME_CHANNEL[] = "Channel";
const char NAME_PACKAGE[] = "Package";
const char NAME_DIE[] = "Die";
const char NAME_PLANE[] = "Plane";
const char NAME_BLOCK[] = "Block";
const char NAME_PAGE[] = "Page";
const char NAME_PAGE_SIZE[] = "PageSize";
const char NAME_USE_MULTI_PLANE_OP[] = "EnableMultiPlaneOperation";
const char NAME_DMA_SPEED[] = "DMASpeed";
const char NAME_DMA_WIDTH[] = "DMAWidth";
const char NAME_FLASH_TYPE[] = "NANDType";

Config::Config() {
  channel = 8;
  package = 4;
  die = 2;
  plane = 1;
  block = 512;
  page = 512;
  pageSize = 16384;
  useMultiPlaneOperation = true;
  dmaSpeed = 400;
  dmaWidth = 8;
  nandType = NAND_MLC;
}

bool Config::setConfig(const char *name, const char *value) {
  bool ret = true;

  if (MATCH_NAME(NAME_CHANNEL)) {
    channel = strtoul(value, nullptr, 10);
  }
  else if (MATCH_NAME(NAME_PACKAGE)) {
    package = strtoul(value, nullptr, 10);
  }
  else if (MATCH_NAME(NAME_DIE)) {
    die = strtoul(value, nullptr, 10);
  }
  else if (MATCH_NAME(NAME_PLANE)) {
    plane = strtoul(value, nullptr, 10);
  }
  else if (MATCH_NAME(NAME_BLOCK)) {
    block = strtoul(value, nullptr, 10);
  }
  else if (MATCH_NAME(NAME_PAGE)) {
    page = strtoul(value, nullptr, 10);
  }
  else if (MATCH_NAME(NAME_PAGE_SIZE)) {
    pageSize = strtoul(value, nullptr, 10);
  }
  else if (MATCH_NAME(NAME_USE_MULTI_PLANE_OP)) {
    useMultiPlaneOperation = convertBool(value);
  }
  else if (MATCH_NAME(NAME_DMA_SPEED)) {
    dmaSpeed = strtoul(value, nullptr, 10);
  }
  else if (MATCH_NAME(NAME_DMA_WIDTH)) {
    dmaWidth = strtoul(value, nullptr, 10);
  }
  else if (MATCH_NAME(NAME_FLASH_TYPE)) {
    nandType = (NAND_TYPE)strtoul(value, nullptr, 10);
  }
  else {
    ret = false;
  }

  return ret;
}

void Config::update() {
  if (dmaWidth & 0x07) {
    Logger::panic("dmaWidth should be multiple of 8.");
  }
}

int64_t Config::readInt(uint32_t idx) {
  int64_t ret = 0;

  switch (idx) {
    case NAND_FLASH_TYPE:
      ret = nandType;
      break;
  }

  return ret;
}

uint64_t Config::readUint(uint32_t idx) {
  uint64_t ret = 0;

  switch (idx) {
    case PAL_CHANNEL:
      ret = channel;
      break;
    case PAL_PACKAGE:
      ret = package;
      break;
    case NAND_DIE:
      ret = die;
      break;
    case NAND_PLANE:
      ret = plane;
      break;
    case NAND_BLOCK:
      ret = block;
      break;
    case NAND_PAGE:
      ret = page;
      break;
    case NAND_PAGE_SIZE:
      ret = pageSize;
      break;
    case NAND_DMA_SPEED:
      ret = dmaSpeed;
      break;
    case NAND_DMA_WIDTH:
      ret = dmaWidth;
      break;
  }

  return ret;
}

float Config::readFloat(uint32_t idx) {
  float ret = 0.f;

  return ret;
}

std::string Config::readString(uint32_t idx) {
  std::string ret("");

  return ret;
}

bool Config::readBoolean(uint32_t idx) {
  bool ret = false;

  switch (idx) {
    case NAND_USE_MULTI_PLANE_OP:
      ret = useMultiPlaneOperation;
      break;
  }

  return ret;
}

}  // namespace PAL

}  // namespace SimpleSSD
