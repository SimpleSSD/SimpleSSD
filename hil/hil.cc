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

#include "hil/hil.hh"

#include "util/algorithm.hh"

namespace SimpleSSD {

namespace HIL {

LPNRange::_LPNRange() : slpn(0), nlp(0) {}

LPNRange::_LPNRange(uint64_t s, uint64_t n) : slpn(s), nlp(n) {}

HIL::HIL(ConfigReader *c) : pConf(c) {
  pICL = new ICL::ICL(pConf);
}

HIL::~HIL() {
  delete pICL;
}

void HIL::read(uint64_t slpn, uint64_t nlp, uint64_t &tick) {
  // TODO: stat

  pICL->read(slpn, nlp, tick);
}

void HIL::read(std::list<LPNRange> &range, uint64_t &tick) {
  uint64_t beginAt;
  uint64_t finishedAt = 0;

  for (auto &iter : range) {
    beginAt = tick;

    read(iter.slpn, iter.nlp, beginAt);
    finishedAt = MAX(finishedAt, beginAt);
  }

  tick = finishedAt;
}

void HIL::write(uint64_t slpn, uint64_t nlp, uint64_t &tick) {
  // TODO: stat

  pICL->write(slpn, nlp, tick);
}

void HIL::write(std::list<LPNRange> &range, uint64_t &tick) {
  uint64_t beginAt;
  uint64_t finishedAt = 0;

  for (auto &iter : range) {
    beginAt = tick;

    write(iter.slpn, iter.nlp, beginAt);
    finishedAt = MAX(finishedAt, beginAt);
  }

  tick = finishedAt;
}

void HIL::flush(uint64_t slpn, uint64_t nlp, uint64_t &tick) {
  // TODO: stat

  pICL->flush(slpn, nlp, tick);
}

void HIL::flush(std::list<LPNRange> &range, uint64_t &tick) {
  uint64_t beginAt;
  uint64_t finishedAt = 0;

  for (auto &iter : range) {
    beginAt = tick;

    flush(iter.slpn, iter.nlp, beginAt);
    finishedAt = MAX(finishedAt, beginAt);
  }

  tick = finishedAt;
}

void HIL::trim(uint64_t slpn, uint64_t nlp, uint64_t &tick) {
  // TODO: stat

  pICL->trim(slpn, nlp, tick);
}

void HIL::trim(std::list<LPNRange> &range, uint64_t &tick) {
  uint64_t beginAt;
  uint64_t finishedAt = 0;

  for (auto &iter : range) {
    beginAt = tick;

    trim(iter.slpn, iter.nlp, beginAt);
    finishedAt = MAX(finishedAt, beginAt);
  }

  tick = finishedAt;
}

void HIL::getLPNInfo(uint64_t &totalLogicalPages, uint32_t &logicalPageSize) {
  pICL->getLPNInfo(totalLogicalPages, logicalPageSize);
}

}  // namespace HIL

}  // namespace SimpleSSD
