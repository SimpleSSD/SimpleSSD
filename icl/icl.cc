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
#include "log/trace.hh"
#include "util/algorithm.hh"
#include "util/def.hh"

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

void ICL::read(Request &req, uint64_t &tick) {
  uint64_t beginAt;
  uint64_t finishedAt = tick;
  uint64_t reqRemain = req.length;
  Request reqInternal;

  reqInternal.reqID = req.reqID;
  reqInternal.offset = req.offset;

  for (uint64_t i = 0; i < req.range.nlp; i++) {
    beginAt = tick;

    reqInternal.reqSubID = i + 1;
    reqInternal.range.slpn = req.range.slpn + i;
    reqInternal.length = MIN(reqRemain, logicalPageSize - reqInternal.offset);
    pCache->read(reqInternal, beginAt);
    reqRemain -= reqInternal.length;
    reqInternal.offset = 0;

    finishedAt = MAX(finishedAt, beginAt);
  }

  Logger::debugprint(Logger::LOG_ICL,
                     "READ  | LPN %" PRIu64 " + %" PRIu64 " | %" PRIu64
                     " - %" PRIu64 " (%" PRIu64 ")",
                     req.range.slpn, req.range.nlp, tick, finishedAt,
                     finishedAt - tick);

  tick = finishedAt;
}

void ICL::write(Request &req, uint64_t &tick) {
  uint64_t beginAt;
  uint64_t finishedAt = tick;
  uint64_t reqRemain = req.length;
  Request reqInternal;

  reqInternal.reqID = req.reqID;
  reqInternal.offset = req.offset;

  for (uint64_t i = 0; i < req.range.nlp; i++) {
    beginAt = tick;

    reqInternal.reqSubID = i + 1;
    reqInternal.range.slpn = req.range.slpn + i;
    reqInternal.length = MIN(reqRemain, logicalPageSize - reqInternal.offset);
    pCache->write(reqInternal, beginAt);
    reqRemain -= reqInternal.length;
    reqInternal.offset = 0;

    finishedAt = MAX(finishedAt, beginAt);
  }

  Logger::debugprint(Logger::LOG_ICL,
                     "WRITE | LPN %" PRIu64 " + %" PRIu64 " | %" PRIu64
                     " - %" PRIu64 " (%" PRIu64 ")",
                     req.range.slpn, req.range.nlp, tick, finishedAt,
                     finishedAt - tick);

  tick = finishedAt;
}

void ICL::flush(Request &req, uint64_t &tick) {
  uint64_t beginAt;
  uint64_t finishedAt = tick;
  uint64_t reqRemain = req.length;
  Request reqInternal;

  reqInternal.reqID = req.reqID;
  reqInternal.offset = req.offset;

  for (uint64_t i = 0; i < req.range.nlp; i++) {
    beginAt = tick;

    reqInternal.reqSubID = i + 1;
    reqInternal.range.slpn = req.range.slpn + i;
    reqInternal.length = MIN(reqRemain, logicalPageSize - reqInternal.offset);
    pCache->flush(reqInternal, beginAt);
    reqRemain -= reqInternal.length;
    reqInternal.offset = 0;

    finishedAt = MAX(finishedAt, beginAt);
  }

  Logger::debugprint(Logger::LOG_ICL,
                     "FLUSH | LPN %" PRIu64 " + %" PRIu64 " | %" PRIu64
                     " - %" PRIu64 " (%" PRIu64 ")",
                     req.range.slpn, req.range.nlp, tick, finishedAt,
                     finishedAt - tick);

  tick = finishedAt;
}

void ICL::trim(Request &req, uint64_t &tick) {
  uint64_t beginAt;
  uint64_t finishedAt = tick;
  uint64_t reqRemain = req.length;
  Request reqInternal;

  reqInternal.reqID = req.reqID;
  reqInternal.offset = req.offset;

  for (uint64_t i = 0; i < req.range.nlp; i++) {
    beginAt = tick;

    reqInternal.reqSubID = i + 1;
    reqInternal.range.slpn = req.range.slpn + i;
    reqInternal.length = MIN(reqRemain, logicalPageSize - reqInternal.offset);
    pCache->trim(reqInternal, beginAt);
    reqRemain -= reqInternal.length;
    reqInternal.offset = 0;

    finishedAt = MAX(finishedAt, beginAt);
  }

  Logger::debugprint(Logger::LOG_ICL,
                     "TRIM  | LPN %" PRIu64 " + %" PRIu64 " | %" PRIu64
                     " - %" PRIu64 " (%" PRIu64 ")",
                     req.range.slpn, req.range.nlp, tick, finishedAt,
                     finishedAt - tick);

  tick = finishedAt;
}

void ICL::format(LPNRange &range, uint64_t &tick) {
  uint64_t beginAt = tick;

  pCache->format(range, tick);

  Logger::debugprint(Logger::LOG_ICL,
                     "FORMAT| LPN %" PRIu64 " + %" PRIu64 " | %" PRIu64
                     " - %" PRIu64 " (%" PRIu64 ")",
                     range.slpn, range.nlp, beginAt, tick,
                     tick - beginAt);
}

void ICL::getLPNInfo(uint64_t &t, uint32_t &s) {
  t = totalLogicalPages;
  s = logicalPageSize;
}

}  // namespace ICL

}  // namespace SimpleSSD
