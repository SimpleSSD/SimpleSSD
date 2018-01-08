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

/*
 * This class wraps old ftl codes by Narges
 */

#include "ftl/ftl_old.hh"

#include "ftl/old/ftl.hh"
#include "ftl/old/ftl_defs.hh"
#include "log/trace.hh"
#include "pal/old/Latency.h"
#include "pal/old/LatencyMLC.h"
#include "pal/old/LatencySLC.h"
#include "pal/old/LatencyTLC.h"
#include "pal/old/PAL2.h"
#include "pal/old/PALStatistics.h"

namespace SimpleSSD {

namespace FTL {

FTLOLD::FTLOLD(Parameter *p, PAL::PAL *l, ConfigReader *c)
    : AbstractFTL(p, l), pConf(&c->ftlConfig) {
  old.physical_block_number = p->totalPhysicalBlocks;
  old.logical_block_number = p->totalLogicalBlocks;
  old.physical_page_number = p->totalPhysicalBlocks * p->pagesInBlock;
  old.logical_page_number = p->totalLogicalBlocks * p->pagesInBlock;
  old.mapping_N = pConf->readUint(FTL_NKMAP_N);
  old.mapping_K = pConf->readUint(FTL_NKMAP_K);
  old.gc_threshold = pConf->readFloat(FTL_GC_THRESHOLD_RATIO);
  old.page_size = 1;
  old.over_provide = pConf->readFloat(FTL_OVERPROVISION_RATIO);
  old.warmup = pConf->readFloat(FTL_WARM_UP_RATIO);
  old.erase_cycle = pConf->readUint(FTL_BAD_BLOCK_THRESHOLD);
  old.page_byte = p->pageSize;
  old.page_per_block = p->pagesInBlock;
  old.ioUnitInPage = p->ioUnitInPage;

  ftl = new ::FTL(&old, l);
}

FTLOLD::~FTLOLD() {
  delete ftl;
}

bool FTLOLD::initialize() {
  return ftl->initialize();
}

void FTLOLD::read(Request &req, uint64_t &tick) {
  uint64_t begin = tick;

  tick = ftl->read(req, begin);

  Logger::debugprint(Logger::LOG_FTL_OLD,
                     "READ  | LPN %" PRIu64 " | %" PRIu64 " - %" PRIu64
                     " (%" PRIu64 ")",
                     req.lpn, begin, tick, tick - begin);
}

void FTLOLD::write(Request &req, uint64_t &tick) {
  uint64_t begin = tick;

  tick = ftl->write(req, begin);

  Logger::debugprint(Logger::LOG_FTL_OLD,
                     "WRITE | LPN %" PRIu64 " | %" PRIu64 " - %" PRIu64
                     " (%" PRIu64 ")",
                     req.lpn, begin, tick, tick - begin);
}

void FTLOLD::trim(Request &req, uint64_t &tick) {
  Logger::debugprint(Logger::LOG_FTL_OLD, "TRIM  | NOT IMPLEMENTED");
}

void FTLOLD::format(LPNRange &range, uint64_t &tick) {
  Logger::debugprint(Logger::LOG_FTL_OLD, "FORMAT| NOT IMPLEMENTED");
}

}  // namespace FTL

}  // namespace SimpleSSD
