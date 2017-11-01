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

#include "ftl/old/Latency.h"
#include "ftl/old/LatencyMLC.h"
#include "ftl/old/LatencySLC.h"
#include "ftl/old/LatencyTLC.h"
#include "ftl/old/PAL2.h"
#include "ftl/old/PALStatistics.h"
#include "ftl/old/ftl.hh"
#include "ftl/old/ftl_defs.hh"

namespace SimpleSSD {

namespace FTL {

FTLOLD::FTLOLD(Parameter *p, PAL::PAL *l, ConfigReader *c)
    : AbstractFTL(p, l), pConf(&c->ftlConfig) {
  switch (c->palConfig.readInt(PAL::NAND_FLASH_TYPE)) {
    case PAL::NAND_SLC:
      lat = new LatencySLC(c->palConfig.readUint(PAL::NAND_DMA_SPEED),
                           c->palConfig.readUint(PAL::NAND_PAGE_SIZE));
      break;
    case PAL::NAND_MLC:
      lat = new LatencyMLC(c->palConfig.readUint(PAL::NAND_DMA_SPEED),
                           c->palConfig.readUint(PAL::NAND_PAGE_SIZE));
      break;
    case PAL::NAND_TLC:
      lat = new LatencyTLC(c->palConfig.readUint(PAL::NAND_DMA_SPEED),
                           c->palConfig.readUint(PAL::NAND_PAGE_SIZE));
      break;
  }

  stats = new PALStatistics(&c->palConfig, lat);
  pal = new PAL2(stats, &c->palConfig, lat);
  old = new ::Parameter();

  old->physical_block_number = p->totalPhysicalBlocks;
  old->logical_block_number = p->totalLogicalBlocks;
  old->physical_page_number = p->totalPhysicalBlocks * p->pagesInBlock;
  old->logical_page_number = p->totalLogicalBlocks * p->pagesInBlock;
  old->mapping_N = pConf->readUint(FTL_NKMAP_N);
  old->mapping_K = pConf->readUint(FTL_NKMAP_K);
  old->gc_threshold = pConf->readFloat(FTL_GC_THRESHOLD_RATIO);
  old->page_size = 1;
  old->over_provide = pConf->readFloat(FTL_OVERPROVISION_RATIO);
  old->warmup = pConf->readFloat(FTL_WARM_UP_RATIO);
  old->erase_cycle = pConf->readUint(FTL_ERASE_CYCLE);
  old->page_byte = p->pageSize;

  ftl = new ::FTL(old, pal);
}

FTLOLD::~FTLOLD() {
  delete ftl;
  delete pal;
  delete stats;
  delete old;
}

bool FTLOLD::initialize() {
  return ftl->initialize();
}

void FTLOLD::read(uint64_t slpn, uint64_t &tick) {
  tick = ftl->read(slpn, 1, tick);
}

void FTLOLD::write(uint64_t slpn, uint64_t &tick) {
  tick = ftl->write(slpn, 1, tick);
}

void FTLOLD::trim(uint64_t slpn, uint64_t &tick) {
  // NOT USED
}

}  // namespace FTL

}  // namespace SimpleSSD
