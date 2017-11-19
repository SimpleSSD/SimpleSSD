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

#include "pal/pal_old.hh"

#include "pal/old/Latency.h"
#include "pal/old/LatencyMLC.h"
#include "pal/old/LatencySLC.h"
#include "pal/old/LatencyTLC.h"
#include "pal/old/PAL2.h"
#include "pal/old/PALStatistics.h"
#include "util/algorithm.hh"

namespace SimpleSSD {

namespace PAL {

PALOLD::PALOLD(Parameter &p, Config &c) : AbstractPAL(p, c) {
  switch (c.readInt(NAND_FLASH_TYPE)) {
    case NAND_SLC:
      lat = new LatencySLC(c.readUint(NAND_DMA_SPEED),
                           c.readUint(NAND_PAGE_SIZE));
      break;
    case NAND_MLC:
      lat = new LatencyMLC(c.readUint(NAND_DMA_SPEED),
                           c.readUint(NAND_PAGE_SIZE));
      break;
    case NAND_TLC:
      lat = new LatencyTLC(c.readUint(NAND_DMA_SPEED),
                           c.readUint(NAND_PAGE_SIZE));
      break;
  }

  stats = new PALStatistics(&c, lat);
  pal = new PAL2(stats, &param, &c, lat);
}

PALOLD::~PALOLD() {
  delete pal;
  delete stats;
  // delete lat;
}

void PALOLD::read(Request &req, uint64_t &tick) {
  uint64_t finishedAt = tick;
  ::Command cmd(tick, 0, OPER_READ, param.superPageSize);
  std::vector<::CPDPBP> list;

  convertCPDPBP(req, list);

  for (auto &iter : list) {
    pal->submit(cmd, iter);

    finishedAt = MAX(finishedAt, cmd.finished);
  }

  tick = finishedAt;
}

void PALOLD::write(Request &req, uint64_t &tick) {
  uint64_t finishedAt = tick;
  ::Command cmd(tick, 0, OPER_WRITE, param.superPageSize);
  std::vector<::CPDPBP> list;

  convertCPDPBP(req, list);

  for (auto &iter : list) {
    pal->submit(cmd, iter);

    finishedAt = MAX(finishedAt, cmd.finished);
  }

  tick = finishedAt;
}

void PALOLD::erase(Request &req, uint64_t &tick) {
  uint64_t finishedAt = tick;
  ::Command cmd(tick, 0, OPER_ERASE, param.superPageSize);
  std::vector<::CPDPBP> list;

  convertCPDPBP(req, list);

  for (auto &iter : list) {
    pal->submit(cmd, iter);

    finishedAt = MAX(finishedAt, cmd.finished);
  }

  tick = finishedAt;
}

void PALOLD::convertCPDPBP(Request &req, std::vector<::CPDPBP> &list) {
  // TODO: add setting for block index disassemble order
  ::CPDPBP addr;

  list.clear();

  addr.Page = req.pageIndex;

  if (conf.readBoolean(NAND_USE_MULTI_PLANE_OP)) {
    addr.Block = req.blockIndex;
    addr.Plane = 0;
  }
  else {
    addr.Block = req.blockIndex / param.plane;
    addr.Plane = req.blockIndex % param.plane;
  }

  for (uint32_t c = 0; c < param.channel; c++) {
    for (uint32_t p = 0; p < param.package; p++) {
      for (uint32_t d = 0; d < param.die; d++) {
        addr.Channel = c;
        addr.Package = 0;
        addr.Die = d;

        list.push_back(addr);
      }
    }
  }
}

}  // namespace PAL

}  // namespace SimpleSSD
