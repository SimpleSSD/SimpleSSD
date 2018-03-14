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

#include "pal/pal.hh"

#include "log/trace.hh"
#include "pal/pal_old.hh"

namespace SimpleSSD {

namespace PAL {

PAL::PAL(ConfigReader *c) : pConf(c) {
  static const char name[4][16] = {"Channel", "Way", "Die", "Plane"};
  uint32_t value[4];
  uint8_t superblock = pConf->palConfig.getSuperblockConfig();

  param.channel = pConf->palConfig.readUint(PAL_CHANNEL);
  param.package = pConf->palConfig.readUint(PAL_PACKAGE);

  param.die = pConf->palConfig.readUint(NAND_DIE);
  param.plane = pConf->palConfig.readUint(NAND_PLANE);
  param.block = pConf->palConfig.readUint(NAND_BLOCK);
  param.page = pConf->palConfig.readUint(NAND_PAGE);
  param.pageSize = pConf->palConfig.readUint(NAND_PAGE_SIZE);
  param.superBlock = param.block;
  param.superPageSize = param.pageSize;

  // Super block includes channel
  if (superblock & INDEX_CHANNEL) {
    param.superPageSize *= param.channel;
    value[0] = param.channel;
  }
  else {
    param.superBlock *= param.channel;
  }

  // Super block includes way (package)
  if (superblock & INDEX_PACKAGE) {
    param.superPageSize *= param.package;
    value[1] = param.package;
  }
  else {
    param.superBlock *= param.package;
  }

  // Super block includes die
  if (superblock & INDEX_DIE) {
    param.superPageSize *= param.die;
    value[2] = param.die;
  }
  else {
    param.superBlock *= param.die;
  }

  // Super block includes plane
  if (pConf->palConfig.readBoolean(NAND_USE_MULTI_PLANE_OP) |
      (superblock & INDEX_PLANE)) {
    param.superPageSize *= param.plane;
    value[3] = param.plane;
  }
  else {
    param.superBlock *= param.plane;
  }

  // Partial I/O tweak
  param.pageInSuperPage = param.superPageSize / param.pageSize;

  // TODO: If PAL revised, this code may not needed
  if (pConf->palConfig.readBoolean(NAND_USE_MULTI_PLANE_OP)) {
    param.pageInSuperPage /= param.plane;
  }

  // Print super block information
  Logger::debugprint(
      Logger::LOG_PAL,
      "Channel |   Way   |   Die   |  Plane  |  Block  |   Page  ");
  Logger::debugprint(Logger::LOG_PAL, "%7u | %7u | %7u | %7u | %7u | %7u",
                     param.channel, param.package, param.die, param.plane,
                     param.block, param.page);
  Logger::debugprint(Logger::LOG_PAL, "Multi-plane mode %s",
                     pConf->palConfig.readBoolean(NAND_USE_MULTI_PLANE_OP)
                         ? "enabled"
                         : "disabled");
  Logger::debugprint(Logger::LOG_PAL, "Superblock multiplier");

  for (int i = 0; i < 4; i++) {
    if (superblock & (1 << i)) {
      Logger::debugprint(Logger::LOG_PAL, "x%u (%s)", value[i], name[i]);
    }
  }

  Logger::debugprint(Logger::LOG_PAL, "Page size %u -> %u", param.pageSize,
                     param.superPageSize);
  Logger::debugprint(
      Logger::LOG_PAL, "Total block count %u -> %u",
      param.channel * param.package * param.die * param.plane * param.block,
      param.superBlock);

  pPAL = new PALOLD(param, c->palConfig);
}

PAL::~PAL() {
  delete pPAL;
}

void PAL::read(Request &req, uint64_t &tick) {
  pPAL->read(req, tick);
}

void PAL::write(Request &req, uint64_t &tick) {
  pPAL->write(req, tick);
}

void PAL::erase(Request &req, uint64_t &tick) {
  pPAL->erase(req, tick);
}

void PAL::copyback(uint32_t blockIndex, uint32_t oldPageIndex,
                   uint32_t newPageIndex, uint64_t &tick) {
  Logger::panic("Copyback not implemented");
}

Parameter *PAL::getInfo() {
  return &param;
}

void PAL::getStats(std::vector<Stats> &list) {
  pPAL->getStats(list);
}

void PAL::getStatValues(std::vector<uint64_t> &values) {
  pPAL->getStatValues(values);
}

void PAL::resetStats() {
  pPAL->resetStats();
}

}  // namespace PAL

}  // namespace SimpleSSD
