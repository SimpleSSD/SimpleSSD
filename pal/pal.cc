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

namespace SimpleSSD {

namespace PAL {

PAL::PAL(ConfigReader *c) : pConf(c) {
  param.channel = pConf->palConfig.readUint(PAL_CHANNEL);
  param.package = pConf->palConfig.readUint(PAL_PACKAGE);

  param.die = pConf->palConfig.readUint(NAND_DIE);
  param.plane = pConf->palConfig.readUint(NAND_PLANE);
  param.block = pConf->palConfig.readUint(NAND_BLOCK);
  param.page = pConf->palConfig.readUint(NAND_PAGE);
  param.pageSize = pConf->palConfig.readUint(NAND_PAGE_SIZE);

  // TODO Make options to setting size and address parse order of super block
  param.superBlock = param.block;
  param.superPage = param.page;
  param.superPageSize =
      param.pageSize * param.channel * param.package * param.die;

  if (pConf->palConfig.readBoolean(NAND_USE_MULTI_PLANE_OP)) {
    param.superPageSize *= param.plane;
  }
  else {
    param.superBlock *= param.plane;
  }
}

PAL::~PAL() {}

void PAL::read(uint32_t blockIndex, uint32_t pageIndex, uint64_t &tick) {
  // TODO implement
}

void PAL::write(uint32_t blockIndex, uint32_t pageIndex, uint64_t &tick) {
  // TODO implement
}

void PAL::erase(uint32_t blockIndex, uint64_t &tick) {
  // TODO implement
}


Parameter *PAL::getInfo() {
  return &param;
}

}  // namespace PAL

}  // namespace SimpleSSD
