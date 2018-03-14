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

#include "ftl/page_mapping.hh"
#include "log/trace.hh"

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
  param.ioUnitInPage = palparam->pageInSuperPage;
  param.pageCountToMaxPerf = palparam->superBlock / palparam->block;

  switch (pConf->ftlConfig.readInt(FTL_MAPPING_MODE)) {
    case PAGE_MAPPING:
      pFTL = new PageMapping(&param, pPAL, pConf);
      break;
  }

  if (param.totalPhysicalBlocks <=
      param.totalLogicalBlocks + param.pageCountToMaxPerf) {
    Logger::panic("FTL Over-Provision Ratio is too small");
  }

  // Print mapping Information
  Logger::debugprint(Logger::LOG_FTL, "Total physical blocks %u",
                     param.totalPhysicalBlocks);
  Logger::debugprint(Logger::LOG_FTL, "Total logical blocks %u",
                     param.totalLogicalBlocks);
  Logger::debugprint(Logger::LOG_FTL, "Logical page size %u", param.pageSize);

  // Initialize pFTL
  pFTL->initialize();
}

FTL::~FTL() {
  delete pPAL;
  delete pFTL;
}

void FTL::read(Request &req, uint64_t &tick) {
  Logger::debugprint(Logger::LOG_FTL, "READ  | LPN %" PRIu64, req.lpn);

  pFTL->read(req, tick);
}

void FTL::write(Request &req, uint64_t &tick) {
  Logger::debugprint(Logger::LOG_FTL, "WRITE | LPN %" PRIu64, req.lpn);

  pFTL->write(req, tick);
}

void FTL::trim(Request &req, uint64_t &tick) {
  Logger::debugprint(Logger::LOG_FTL, "TRIM  | LPN %" PRIu64, req.lpn);

  pFTL->trim(req, tick);
}

void FTL::format(LPNRange &range, uint64_t &tick) {
  pFTL->format(range, tick);
}

Parameter *FTL::getInfo() {
  return &param;
}

uint64_t FTL::getUsedPageCount() {
  return pFTL->getStatus()->mappedLogicalPages;
}

void FTL::getStats(std::vector<Stats> &list) {
  pFTL->getStats(list);
  pPAL->getStats(list);
}

void FTL::getStatValues(std::vector<uint64_t> &values) {
  pFTL->getStatValues(values);
  pPAL->getStatValues(values);
}

void FTL::resetStats() {
  pFTL->resetStats();
  pPAL->resetStats();
}

}  // namespace FTL

}  // namespace SimpleSSD
