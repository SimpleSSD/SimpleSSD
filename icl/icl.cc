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

#include "icl/icl.hh"

#include "icl/generic_cache.hh"
#include "util/algorithm.hh"

namespace SimpleSSD {

namespace ICL {

ICL::ICL(ConfigReader *c) : pConf(c) {
  pFTL = new FTL::FTL(pConf);

  FTL::Parameter *param = pFTL->getInfo();

  totalLogicalPages = param->totalLogicalBlocks * param->pagesInBlock;
  logicalPageSize = param->pageSize;

  pCache = new GenericCache(pConf, pFTL);
}

ICL::~ICL() {
  delete pCache;
  delete pFTL;
}

void ICL::read(uint64_t slpn, uint64_t nlp, uint64_t &tick) {
  uint64_t beginAt;
  uint64_t finishedAt = 0;

  for (uint64_t i = 0; i < nlp; i++) {
    beginAt = tick;
    pCache->read(slpn + i, beginAt);
    finishedAt = MAX(finishedAt, beginAt);
  }

  tick = finishedAt;
}

void ICL::write(uint64_t slpn, uint64_t nlp, uint64_t &tick) {
  uint64_t beginAt;
  uint64_t finishedAt = 0;

  for (uint64_t i = 0; i < nlp; i++) {
    beginAt = tick;
    pCache->write(slpn + i, beginAt);
    finishedAt = MAX(finishedAt, beginAt);
  }

  tick = finishedAt;
}

void ICL::flush(uint64_t slpn, uint64_t nlp, uint64_t &tick) {
  uint64_t beginAt;
  uint64_t finishedAt = 0;

  for (uint64_t i = 0; i < nlp; i++) {
    beginAt = tick;
    pCache->flush(slpn + i, beginAt);
    finishedAt = MAX(finishedAt, beginAt);
  }

  tick = finishedAt;
}

void ICL::trim(uint64_t slpn, uint64_t nlp, uint64_t &tick) {
  uint64_t beginAt;
  uint64_t finishedAt = 0;

  for (uint64_t i = 0; i < nlp; i++) {
    beginAt = tick;
    pCache->trim(slpn + i, beginAt);
    finishedAt = MAX(finishedAt, beginAt);
  }

  tick = finishedAt;
}

void ICL::getLPNInfo(uint64_t &t, uint32_t &s) {
  t = totalLogicalPages;
  s = logicalPageSize;
}

}  // namespace ICL

}  // namespace SimpleSSD
