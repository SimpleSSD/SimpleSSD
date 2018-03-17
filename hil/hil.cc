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

HIL::HIL(ConfigReader &c) : conf(c), reqCount(0) {
  pICL = new ICL::ICL(conf);

  memset(&stat, 0, sizeof(stat));

  completionEvent = allocate([this](uint64_t) { completion(); });
}

HIL::~HIL() {
  delete pICL;
}

void HIL::read(Request &req) {
  uint64_t beginAt = getTick();
  uint64_t tick = beginAt;

  req.reqID = ++reqCount;

  debugprint(LOG_HIL,
             "READ  | REQ %7u | LCA %" PRIu64 " + %" PRIu64 " | BYTE %" PRIu64
             " + %" PRIu64,
             req.reqID, req.range.slpn, req.range.nlp, req.offset, req.length);

  ICL::Request reqInternal(req);
  pICL->read(reqInternal, tick);

  stat.request[0]++;
  stat.iosize[0] += req.length;
  updateBusyTime(0, beginAt, tick);
  updateBusyTime(2, beginAt, tick);

  req.finishedAt = tick;
  completionQueue.push(req);

  updateCompletion();
}

void HIL::write(Request &req) {
  uint64_t beginAt = getTick();
  uint64_t tick = beginAt;

  req.reqID = ++reqCount;

  debugprint(LOG_HIL,
             "WRITE | REQ %7u | LCA %" PRIu64 " + %" PRIu64 " | BYTE %" PRIu64
             " + %" PRIu64,
             req.reqID, req.range.slpn, req.range.nlp, req.offset, req.length);

  ICL::Request reqInternal(req);
  pICL->write(reqInternal, tick);

  stat.request[1]++;
  stat.iosize[1] += req.length;
  updateBusyTime(1, beginAt, tick);
  updateBusyTime(2, beginAt, tick);

  req.finishedAt = tick;
  completionQueue.push(req);

  updateCompletion();
}

void HIL::flush(Request &req) {
  uint64_t tick = getTick();

  // TODO: stat

  req.reqID = ++reqCount;

  debugprint(LOG_HIL, "FLUSH | REQ %7u | LCA %" PRIu64 " + %" PRIu64, req.reqID,
             req.range.slpn, req.range.nlp);

  ICL::Request reqInternal(req);
  pICL->flush(reqInternal, tick);

  req.finishedAt = tick;
  completionQueue.push(req);

  updateCompletion();
}

void HIL::trim(Request &req) {
  uint64_t tick = getTick();

  // TODO: stat

  req.reqID = ++reqCount;

  debugprint(LOG_HIL, "TRIM  | REQ %7u | LCA %" PRIu64 " + %" PRIu64, req.reqID,
             req.range.slpn, req.range.nlp);

  ICL::Request reqInternal(req);
  pICL->trim(reqInternal, tick);

  req.finishedAt = tick;
  completionQueue.push(req);

  updateCompletion();
}

void HIL::format(Request &req, bool erase) {
  uint64_t tick = getTick();

  debugprint(LOG_HIL, "FORMAT| LCA %" PRIu64 " + %" PRIu64, req.range.slpn,
             req.range.nlp);

  if (erase) {
    pICL->format(req.range, tick);
  }
  else {
    ICL::Request reqInternal(req);

    pICL->trim(reqInternal, tick);
  }

  req.finishedAt = tick;
  completionQueue.push(req);

  updateCompletion();
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

void HIL::updateCompletion() {
  if (completionQueue.size() > 0) {
    schedule(completionEvent, completionQueue.top().finishedAt);
  }
}

void HIL::completion() {
  uint64_t tick = getTick();

  while (completionQueue.size() > 0) {
    auto &req = completionQueue.top();

    if (req.finishedAt <= tick) {
      req.function(tick, req.context);

      completionQueue.pop();
    }
    else {
      break;
    }
  }

  updateCompletion();
}

void HIL::getStatList(std::vector<Stats> &list, std::string prefix) {
  Stats temp;

  temp.name = prefix + "read.request_count";
  temp.desc = "Read request count";
  list.push_back(temp);

  temp.name = prefix + "read.bytes";
  temp.desc = "Read data size in byte";
  list.push_back(temp);

  temp.name = prefix + "read.busy";
  temp.desc = "Device busy time when read";
  list.push_back(temp);

  temp.name = prefix + "write.request_count";
  temp.desc = "Write request count";
  list.push_back(temp);

  temp.name = prefix + "write.bytes";
  temp.desc = "Write data size in byte";
  list.push_back(temp);

  temp.name = prefix + "write.busy";
  temp.desc = "Device busy time when write";
  list.push_back(temp);

  temp.name = prefix + "request_count";
  temp.desc = "Total request count";
  list.push_back(temp);

  temp.name = prefix + "bytes";
  temp.desc = "Total data size in byte";
  list.push_back(temp);

  temp.name = prefix + "busy";
  temp.desc = "Total device busy time";
  list.push_back(temp);

  pICL->getStatList(list, prefix);
}

void HIL::getStatValues(std::vector<double> &values) {
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

void HIL::resetStatValues() {
  memset(&stat, 0, sizeof(stat));

  pICL->resetStatValues();
}

}  // namespace HIL

}  // namespace SimpleSSD
