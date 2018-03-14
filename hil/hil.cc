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

#include "log/trace.hh"
#include "util/algorithm.hh"

namespace SimpleSSD {

namespace HIL {

HIL::HIL(ConfigReader *c) : pConf(c), reqCount(0) {
  pICL = new ICL::ICL(pConf);

  memset(&stat, 0, sizeof(stat));
}

HIL::~HIL() {
  delete pICL;
}

void HIL::read(ICL::Request &req, uint64_t &tick) {
  uint64_t beginAt = tick;

  req.reqID = ++reqCount;

  Logger::debugprint(Logger::LOG_HIL,
                     "READ  | REQ %7u | LCA %" PRIu64 " + %" PRIu64
                     " | BYTE %" PRIu64 " + %" PRIu64,
                     req.reqID, req.range.slpn, req.range.nlp, req.offset,
                     req.length);

  pICL->read(req, tick);

  stat.request[0]++;
  stat.iosize[0] += req.length;
  updateBusyTime(0, beginAt, tick);
  updateBusyTime(2, beginAt, tick);
}

void HIL::write(ICL::Request &req, uint64_t &tick) {
  uint64_t beginAt = tick;

  req.reqID = ++reqCount;

  Logger::debugprint(Logger::LOG_HIL,
                     "WRITE | REQ %7u | LCA %" PRIu64 " + %" PRIu64
                     " | BYTE %" PRIu64 " + %" PRIu64,
                     req.reqID, req.range.slpn, req.range.nlp, req.offset,
                     req.length);

  pICL->write(req, tick);

  stat.request[1]++;
  stat.iosize[1] += req.length;
  updateBusyTime(1, beginAt, tick);
  updateBusyTime(2, beginAt, tick);
}

void HIL::flush(ICL::Request &req, uint64_t &tick) {
  // TODO: stat

  req.reqID = ++reqCount;

  Logger::debugprint(Logger::LOG_HIL,
                     "FLUSH | REQ %7u | LCA %" PRIu64 " + %" PRIu64, req.reqID,
                     req.range.slpn, req.range.nlp);

  pICL->flush(req, tick);
}

void HIL::trim(ICL::Request &req, uint64_t &tick) {
  // TODO: stat

  req.reqID = ++reqCount;

  Logger::debugprint(Logger::LOG_HIL,
                     "TRIM  | REQ %7u | LCA %" PRIu64 " + %" PRIu64, req.reqID,
                     req.range.slpn, req.range.nlp);

  pICL->trim(req, tick);
}

void HIL::format(LPNRange &range, bool erase, uint64_t &tick) {
  Logger::debugprint(Logger::LOG_HIL, "FORMAT| LCA %" PRIu64 " + %" PRIu64,
                     range.slpn, range.nlp);

  if (erase) {
    pICL->format(range, tick);
  }
  else {
    ICL::Request req;

    req.range = range;

    pICL->trim(req, tick);
  }
}

void HIL::getLPNInfo(uint64_t &totalLogicalPages, uint32_t &logicalPageSize) {
  pICL->getLPNInfo(totalLogicalPages, logicalPageSize);
}

uint64_t HIL::getUsedPageCount() {
  return pICL->getUsedPageCount();
}

void HIL::updateBusyTime(int idx, uint64_t begin, uint64_t end) {
  if (end <= stat.lastBusyAt[idx]) {
    return;
  }

  if (begin < stat.lastBusyAt[idx]) {
    stat.busy[idx] += end - stat.lastBusyAt[idx];
  }
  else {
    stat.busy[idx] += end - begin;
  }

  stat.lastBusyAt[idx] = end;
}

void HIL::getStats(std::vector<Stats> &list) {
  Stats temp;

  temp.name = "hil.read.request_count";
  temp.desc = "Read request count";
  list.push_back(temp);

  temp.name = "hil.read.bytes";
  temp.desc = "Read data size in byte";
  list.push_back(temp);

  temp.name = "hil.read.busy";
  temp.desc = "Device busy time when read";
  list.push_back(temp);

  temp.name = "hil.write.request_count";
  temp.desc = "Write request count";
  list.push_back(temp);

  temp.name = "hil.write.bytes";
  temp.desc = "Write data size in byte";
  list.push_back(temp);

  temp.name = "hil.write.busy";
  temp.desc = "Device busy time when write";
  list.push_back(temp);

  temp.name = "hil.request_count";
  temp.desc = "Total request count";
  list.push_back(temp);

  temp.name = "hil.bytes";
  temp.desc = "Total data size in byte";
  list.push_back(temp);

  temp.name = "hil.busy";
  temp.desc = "Total device busy time";
  list.push_back(temp);

  pICL->getStats(list);
}

void HIL::getStatValues(std::vector<uint64_t> &values) {
  values.push_back(stat.request[0]);
  values.push_back(stat.iosize[0]);
  values.push_back(stat.busy[0]);
  values.push_back(stat.request[1]);
  values.push_back(stat.iosize[1]);
  values.push_back(stat.busy[1]);
  values.push_back(stat.request[0] + stat.request[1]);
  values.push_back(stat.iosize[0] + stat.iosize[1]);
  values.push_back(stat.busy[2]);

  pICL->getStatValues(values);
}

void HIL::resetStats() {
  memset(&stat, 0, sizeof(stat));

  pICL->resetStats();
}

}  // namespace HIL

}  // namespace SimpleSSD
