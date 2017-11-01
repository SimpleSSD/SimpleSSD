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

#include "ftl/ftl.hh"

#include "ftl/abstract_ftl.hh"

namespace SimpleSSD {

namespace FTL {

FTL::FTL(ConfigReader *c) : pConf(c) {
  PAL::Parameter *palparam;

  pPAL = new PAL::PAL(pConf);
  palparam = pPAL->getInfo();

  param.totalPhysicalBlocks = palparam->superBlock;
  param.totalLogicalBlocks =
      palparam->superBlock *
      (1 - pConf->ftlConfig.readFloat(FTL_OVERPROVISION_RATIO));
  param.pagesInBlock = palparam->page;
  param.pageSize = palparam->superPageSize;

  // TODO allocated pFTL
  // Initialize pFTL
}

FTL::~FTL() {
  delete pPAL;
  delete pFTL;
}

void FTL::read(uint64_t lpn, uint64_t &tick) {
  pFTL->read(lpn, tick);
}

void FTL::write(uint64_t lpn, uint64_t &tick) {
  pFTL->write(lpn, tick);
}

void FTL::trim(uint64_t lpn, uint64_t &tick) {
  pFTL->trim(lpn, tick);
}

Parameter *FTL::getInfo() {
  return &param;
}

}  // namespace FTL

}  // namespace SimpleSSD
