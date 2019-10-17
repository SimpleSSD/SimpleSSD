// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "fil/pal/pal_wrapper.hh"

#include <sstream>

#include "fil/pal/Latency.h"
#include "fil/pal/LatencyMLC.h"
#include "fil/pal/LatencySLC.h"
#include "fil/pal/LatencyTLC.h"
#include "fil/pal/PAL2.h"
#include "fil/pal/PALStatistics.h"
#include "util/algorithm.hh"

#define FLUSH_PERIOD 100000000000ull  // 0.1sec
#define FLUSH_RANGE 10000000000ull    // 0.01sec

namespace SimpleSSD::FIL {

PALOLD::PALOLD(ObjectData &o) : AbstractNVM(o), lastResetTick(0) {
  param = object.config->getNANDStructure();

  memset(&stat, 0, sizeof(stat));

  switch (param->type) {
    case Config::NANDType::SLC:
      lat = new LatencySLC(object.config);
      break;
    case Config::NANDType::MLC:
      lat = new LatencyMLC(object.config);
      break;
    case Config::NANDType::TLC:
      lat = new LatencyTLC(object.config);
      break;
  }

  Convert convert(object);
  convertCPDPBP = convert.getConvertion();

  stats = new PALStatistics(object.config, lat);
  pal = new PAL2(stats, object.config, lat);

  completeEvent = createEvent([this](uint64_t t, uint64_t) { completion(t); },
                              "FIL::PALOLD::completeEvent");

  // We will periodically flush timeslot for saving memory
  flushEvent = createEvent(
      [this](uint64_t tick, uint64_t) {
        pal->FlushFreeSlots(tick - FLUSH_RANGE);
        pal->FlushTimeSlots(tick - FLUSH_RANGE);

        scheduleRel(flushEvent, 0ull, FLUSH_PERIOD);
      },
      "FIL::PALOLD::flushEvent");
  scheduleRel(flushEvent, 0ull, FLUSH_PERIOD);
}

PALOLD::~PALOLD() {
  delete pal;
  delete stats;
  delete lat;
}

void PALOLD::enqueue(Request &&req) {
  Complete cplt;
  ::CPDPBP addr;

  cplt.id = req.id;
  cplt.eid = req.eid;
  cplt.data = req.data;
  cplt.beginAt = getTick();

  convertCPDPBP(req, addr);

  switch (req.opcode) {
    case Operation::Read: {
      ::Command cmd(getTick(), 0, OPER_READ, param->pageSize);

      printCPDPBP(addr, "READ");

      pal->submit(cmd, addr);
      stat.readCount++;

      cplt.finishedAt = cmd.finished;
    } break;
    case Operation::Program: {
      ::Command cmd(getTick(), 0, OPER_WRITE, param->pageSize);

      printCPDPBP(addr, "WRITE");

      pal->submit(cmd, addr);
      stat.writeCount++;

      cplt.finishedAt = cmd.finished;
    } break;
    case Operation::Erase: {
      ::Command cmd(getTick(), 0, OPER_ERASE, param->pageSize);

      printCPDPBP(addr, "ERASE");

      pal->submit(cmd, addr);
      stat.eraseCount++;

      cplt.finishedAt = cmd.finished;
    } break;
    default:
      panic("Copyback not supported in PAL.");
      break;
  }

  reschedule(std::move(cplt));
}

void PALOLD::printCPDPBP(::CPDPBP &addr, const char *prefix) {
  debugprint(Log::DebugID::FIL_PALOLD,
             "%-5s | C %5u | W %5u | D %5u | P %5u | B %5u | P %5u", prefix,
             addr.Channel, addr.Package, addr.Die, addr.Plane, addr.Block,
             addr.Page);
}

void PALOLD::reschedule(Complete &&cplt) {
  // Find insertion slot
  auto iter = completionQueue.begin();

  for (; iter != completionQueue.end(); ++iter) {
    if (iter->finishedAt > cplt.finishedAt) {
      break;
    }
  }

  completionQueue.emplace(iter, cplt);

  scheduleAbs(completeEvent, 0ull, completionQueue.front().finishedAt);
}

void PALOLD::completion(uint64_t) {
  Complete cplt = std::move(completionQueue.front());
  completionQueue.pop_front();

  scheduleNow(cplt.eid, cplt.data);

  if (completionQueue.size() > 0) {
    scheduleAbs(completeEvent, 0ull, completionQueue.front().finishedAt);
  }
}

void PALOLD::getStatList(std::vector<Stat> &list, std::string prefix) noexcept {
  Stat temp;

  temp.name = prefix + "energy.read";
  temp.desc = "Consumed energy by NAND read operation (uJ)";
  list.push_back(temp);

  temp.name = prefix + "energy.program";
  temp.desc = "Consumed energy by NAND program operation (uJ)";
  list.push_back(temp);

  temp.name = prefix + "energy.erase";
  temp.desc = "Consumed energy by NAND erase operation (uJ)";
  list.push_back(temp);

  temp.name = prefix + "energy.total";
  temp.desc = "Total consumed energy by NAND (uJ)";
  list.push_back(temp);

  temp.name = prefix + "power";
  temp.desc = "Average power consumed by NAND (uW)";
  list.push_back(temp);

  temp.name = prefix + "read.count";
  temp.desc = "Total read operation count";
  list.push_back(temp);

  temp.name = prefix + "program.count";
  temp.desc = "Total program operation count";
  list.push_back(temp);

  temp.name = prefix + "erase.count";
  temp.desc = "Total erase operation count";
  list.push_back(temp);

  temp.name = prefix + "read.bytes";
  temp.desc = "Total read operation bytes";
  list.push_back(temp);

  temp.name = prefix + "program.bytes";
  temp.desc = "Total program operation bytes";
  list.push_back(temp);

  temp.name = prefix + "erase.bytes";
  temp.desc = "Total erase operation bytes";
  list.push_back(temp);

  temp.name = prefix + "read.time.dma0.wait";
  temp.desc = "Average dma0 wait time of read";
  list.push_back(temp);

  temp.name = prefix + "read.time.dma0";
  temp.desc = "Average dma0 time of read";
  list.push_back(temp);

  temp.name = prefix + "read.time.mem";
  temp.desc = "Average memory operation time of read";
  list.push_back(temp);

  temp.name = prefix + "read.time.dma1.wait";
  temp.desc = "Average dma1 wait time of read";
  list.push_back(temp);

  temp.name = prefix + "read.time.dma1";
  temp.desc = "Average dma1 time of read";
  list.push_back(temp);

  temp.name = prefix + "read.time.total";
  temp.desc = "Average time of read";
  list.push_back(temp);

  temp.name = prefix + "program.time.dma0.wait";
  temp.desc = "Average dma0 wait time of program";
  list.push_back(temp);

  temp.name = prefix + "program.time.dma0";
  temp.desc = "Average dma0 time of program";
  list.push_back(temp);

  temp.name = prefix + "program.time.mem";
  temp.desc = "Average memory operation time of program";
  list.push_back(temp);

  temp.name = prefix + "program.time.dma1.wait";
  temp.desc = "Average dma1 wait time of program";
  list.push_back(temp);

  temp.name = prefix + "program.time.dma1";
  temp.desc = "Average dma1 time of program";
  list.push_back(temp);

  temp.name = prefix + "program.time.total";
  temp.desc = "Average time of program";
  list.push_back(temp);

  temp.name = prefix + "erase.time.dma0.wait";
  temp.desc = "Average dma0 wait time of erase";
  list.push_back(temp);

  temp.name = prefix + "erase.time.dma0";
  temp.desc = "Average dma0 time of erase";
  list.push_back(temp);

  temp.name = prefix + "erase.time.mem";
  temp.desc = "Average memory operation time of erase";
  list.push_back(temp);

  temp.name = prefix + "erase.time.dma1.wait";
  temp.desc = "Average dma1 wait time of erase";
  list.push_back(temp);

  temp.name = prefix + "erase.time.dma1";
  temp.desc = "Average dma1 time of erase";
  list.push_back(temp);

  temp.name = prefix + "erase.time.total";
  temp.desc = "Average time of erase";
  list.push_back(temp);

  temp.name = prefix + "channel.time.active";
  temp.desc = "Average active time of all channels";
  list.push_back(temp);

  temp.name = prefix + "die.time.active";
  temp.desc = "Average active time of all dies";
  list.push_back(temp);
}

void PALOLD::getStatValues(std::vector<double> &values) noexcept {
  PALStatistics::OperStats energy;
  PALStatistics::OperStats ticks;
  PALStatistics::ActiveTime active;
  PALStatistics::Breakdown breakdown;
  double elapsedTick = (double)(getTick() - lastResetTick);

  stats->getEnergyStat(energy);
  stats->getTickStat(ticks);

  values.push_back(energy.read);
  values.push_back(energy.write);
  values.push_back(energy.erase);
  values.push_back(energy.total);

  // uW = uJ / ps * 1e+12
  energy.total /= elapsedTick / 1e+12;
  values.push_back(energy.total);

  values.push_back(stat.readCount);
  values.push_back(stat.writeCount);
  values.push_back(stat.eraseCount);

  values.push_back(stat.readCount * param->pageSize);
  values.push_back(stat.writeCount * param->pageSize);
  values.push_back(stat.eraseCount * param->pageSize * param->page);

  stats->getReadBreakdown(breakdown);
  values.push_back(breakdown.dma0wait);
  values.push_back(breakdown.dma0);
  values.push_back(breakdown.mem);
  values.push_back(breakdown.dma1wait);
  values.push_back(breakdown.dma1);
  values.push_back(ticks.read);

  stats->getWriteBreakdown(breakdown);
  values.push_back(breakdown.dma0wait);
  values.push_back(breakdown.dma0);
  values.push_back(breakdown.mem);
  values.push_back(breakdown.dma1wait);
  values.push_back(breakdown.dma1);
  values.push_back(ticks.write);

  stats->getEraseBreakdown(breakdown);
  values.push_back(breakdown.dma0wait);
  values.push_back(breakdown.dma0);
  values.push_back(breakdown.mem);
  values.push_back(breakdown.dma1wait);
  values.push_back(breakdown.dma1);
  values.push_back(ticks.erase);

  stats->getChannelActiveTimeAll(active);
  values.push_back(active.average);

  stats->getDieActiveTimeAll(active);
  values.push_back(active.average);
}

void PALOLD::resetStatValues() noexcept {
  stats->ResetStats();
  lastResetTick = getTick();

  memset(&stat, 0, sizeof(stat));
}

void PALOLD::createCheckpoint(std::ostream &out) const noexcept {
  BACKUP_EVENT(out, flushEvent);
  BACKUP_SCALAR(out, lastResetTick);
  BACKUP_BLOB(out, &stat, sizeof(stat));
  BACKUP_EVENT(out, completeEvent);

  uint64_t size = completionQueue.size();
  BACKUP_SCALAR(out, size);

  for (auto &iter : completionQueue) {
    BACKUP_SCALAR(out, iter.id);
    BACKUP_EVENT(out, iter.eid);
    BACKUP_SCALAR(out, iter.data);
    BACKUP_SCALAR(out, iter.beginAt);
    BACKUP_SCALAR(out, iter.finishedAt);
  }

  lat->backup(out);
  stats->backup(out);
  pal->backup(out);
}

void PALOLD::restoreCheckpoint(std::istream &in) noexcept {
  RESTORE_EVENT(in, flushEvent);
  RESTORE_SCALAR(in, lastResetTick);
  RESTORE_BLOB(in, &stat, sizeof(stat));
  RESTORE_EVENT(in, completeEvent);

  uint64_t size;
  RESTORE_SCALAR(in, size);

  for (uint64_t i = 0; i < size; i++) {
    Complete tmp;

    RESTORE_SCALAR(in, tmp.id);
    RESTORE_EVENT(in, tmp.eid);
    RESTORE_SCALAR(in, tmp.data);
    RESTORE_SCALAR(in, tmp.beginAt);
    RESTORE_SCALAR(in, tmp.finishedAt);

    completionQueue.emplace_back(tmp);
  }

  lat->restore(in);
  stats->restore(in);
  pal->restore(in);
}

}  // namespace SimpleSSD::FIL
