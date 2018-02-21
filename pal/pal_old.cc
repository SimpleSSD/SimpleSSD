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

#include <sstream>

#include "log/trace.hh"
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
  Config::NANDTiming *pTiming = c.getNANDTiming();

  switch (c.readInt(NAND_FLASH_TYPE)) {
    case NAND_SLC:
      lat = new LatencySLC(*pTiming);
      break;
    case NAND_MLC:
      lat = new LatencyMLC(*pTiming);
      break;
    case NAND_TLC:
      lat = new LatencyTLC(*pTiming);
      break;
  }

  Logger::debugprint(Logger::LOG_PAL_OLD, "NAND timing:");
  Logger::debugprint(Logger::LOG_PAL_OLD, "Operation |     LSB    |     CSB   "
                                          " |     MSB    |    DMA 0   |    DMA "
                                          "1");
  Logger::debugprint(Logger::LOG_PAL_OLD,
                     "   READ   | %10" PRIu64 " | %10" PRIu64 " | %10" PRIu64
                     " | %10" PRIu64 " | %10" PRIu64,
                     pTiming->lsb.read, pTiming->csb.read, pTiming->msb.read,
                     pTiming->dma0.read, pTiming->dma1.read);
  Logger::debugprint(Logger::LOG_PAL_OLD,
                     "   WRITE  | %10" PRIu64 " | %10" PRIu64 " | %10" PRIu64
                     " | %10" PRIu64 " | %10" PRIu64,
                     pTiming->lsb.write, pTiming->csb.write, pTiming->msb.write,
                     pTiming->dma0.write, pTiming->dma1.write);
  Logger::debugprint(Logger::LOG_PAL_OLD,
                     "   ERASE  |                           %10" PRIu64
                     " | %10" PRIu64 " | %10" PRIu64,
                     pTiming->erase, pTiming->dma0.erase, pTiming->dma1.erase);

  stats = new PALStatistics(&c, lat);
  pal = new PAL2(stats, &param, &c, lat);
}

PALOLD::~PALOLD() {
  delete pal;
  delete stats;
  delete lat;
}

void PALOLD::read(Request &req, uint64_t &tick) {
  uint64_t finishedAt = tick;
  ::Command cmd(tick, 0, OPER_READ, param.superPageSize);
  std::vector<::CPDPBP> list;

  printPPN(req, "READ");

  convertCPDPBP(req, list);

  for (auto &iter : list) {
    printCPDPBP(iter, "READ");

    pal->submit(cmd, iter);

    finishedAt = MAX(finishedAt, cmd.finished);
  }

  tick = finishedAt;
}

void PALOLD::write(Request &req, uint64_t &tick) {
  uint64_t finishedAt = tick;
  ::Command cmd(tick, 0, OPER_WRITE, param.superPageSize);
  std::vector<::CPDPBP> list;

  printPPN(req, "WRITE");

  convertCPDPBP(req, list);

  for (auto &iter : list) {
    printCPDPBP(iter, "WRITE");

    pal->submit(cmd, iter);

    finishedAt = MAX(finishedAt, cmd.finished);
  }

  tick = finishedAt;
}

void PALOLD::erase(Request &req, uint64_t &tick) {
  uint64_t finishedAt = tick;
  ::Command cmd(tick, 0, OPER_ERASE, param.superPageSize);
  std::vector<::CPDPBP> list;

  printPPN(req, "ERASE");

  convertCPDPBP(req, list);

  for (auto &iter : list) {
    printCPDPBP(iter, "ERASE");

    pal->submit(cmd, iter);

    finishedAt = MAX(finishedAt, cmd.finished);
  }

  tick = finishedAt;
}

void PALOLD::convertCPDPBP(Request &req, std::vector<::CPDPBP> &list) {
  ::CPDPBP addr;
  static uint32_t pageAllocation = conf.getPageAllocationConfig();
  static uint8_t superblock = conf.getSuperblockConfig();
  static bool useMultiplaneOP = conf.readBoolean(NAND_USE_MULTI_PLANE_OP);
  static uint32_t pageInSuperPage = param.pageInSuperPage;
  uint32_t value[4];
  uint32_t *ptr[4];
  uint64_t tmp = req.blockIndex;
  int count = 0;

  if (req.ioFlag.size() != pageInSuperPage) {
    Logger::panic("Invalid size of I/O flag");
  }

  list.clear();

  addr.Plane = 0;

  for (int i = 0; i < 4; i++) {
    uint8_t idx = (pageAllocation >> (i * 8)) & 0xFF;

    switch (idx) {
      case INDEX_CHANNEL:
        if (superblock & INDEX_CHANNEL) {
          value[count] = param.channel;
          ptr[count++] = &addr.Channel;
        }
        else {
          addr.Channel = tmp % param.channel;
          tmp /= param.channel;
        }

        break;
      case INDEX_PACKAGE:
        if (superblock & INDEX_PACKAGE) {
          value[count] = param.package;
          ptr[count++] = &addr.Package;
        }
        else {
          addr.Package = tmp % param.package;
          tmp /= param.package;
        }

        break;
      case INDEX_DIE:
        if (superblock & INDEX_DIE) {
          value[count] = param.die;
          ptr[count++] = &addr.Die;
        }
        else {
          addr.Die = tmp % param.die;
          tmp /= param.die;
        }

        break;
      case INDEX_PLANE:
        if (!useMultiplaneOP) {
          if (superblock & INDEX_PLANE) {
            value[count] = param.plane;
            ptr[count++] = &addr.Plane;
          }
          else {
            addr.Plane = tmp % param.plane;
            tmp /= param.plane;
          }
        }

        break;
      default:
        break;
    }
  }

  addr.Block = tmp;
  addr.Page = req.pageIndex;

  // Index of ioFlag
  tmp = 0;

  if (count == 4) {
    for (uint32_t i = 0; i < value[3]; i++) {
      for (uint32_t j = 0; j < value[2]; j++) {
        for (uint32_t k = 0; k < value[1]; k++) {
          for (uint32_t l = 0; l < value[0]; l++) {
            if (req.ioFlag.test(tmp++)) {
              *ptr[0] = l;
              *ptr[1] = k;
              *ptr[2] = j;
              *ptr[3] = i;

              list.push_back(addr);
            }
          }
        }
      }
    }
  }
  else if (count == 3) {
    for (uint32_t j = 0; j < value[2]; j++) {
      for (uint32_t k = 0; k < value[1]; k++) {
        for (uint32_t l = 0; l < value[0]; l++) {
          if (req.ioFlag.test(tmp++)) {
            *ptr[0] = l;
            *ptr[1] = k;
            *ptr[2] = j;

            list.push_back(addr);
          }
        }
      }
    }
  }
  else if (count == 2) {
    for (uint32_t k = 0; k < value[1]; k++) {
      for (uint32_t l = 0; l < value[0]; l++) {
        if (req.ioFlag.test(tmp++)) {
          *ptr[0] = l;
          *ptr[1] = k;

          list.push_back(addr);
        }
      }
    }
  }
  else if (count == 1) {
    for (uint32_t l = 0; l < value[0]; l++) {
      if (req.ioFlag.test(tmp++)) {
        *ptr[0] = l;

        list.push_back(addr);
      }
    }
  }
  else {
    if (req.ioFlag.test(tmp++)) {
      list.push_back(addr);
    }
  }

  if (tmp != pageInSuperPage) {
    Logger::panic("I/O flag size != # pages in super page");
  }
}

void PALOLD::printCPDPBP(::CPDPBP &addr, const char *prefix) {
  Logger::debugprint(Logger::LOG_PAL_OLD,
                     "%-5s | C %5u | W %5u | D %5u | P %5u | B %5u | P %5u",
                     prefix, addr.Channel, addr.Package, addr.Die, addr.Plane,
                     addr.Block, addr.Page);
}

void PALOLD::printPPN(Request &req, const char *prefix) {
  Logger::debugprint(Logger::LOG_PAL_OLD, "%-5s | Block %u | Page %u", prefix,
                     req.blockIndex, req.pageIndex);
}

}  // namespace PAL

}  // namespace SimpleSSD
