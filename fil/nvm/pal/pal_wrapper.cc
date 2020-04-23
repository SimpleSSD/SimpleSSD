// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "fil/nvm/pal/pal_wrapper.hh"

#include <sstream>

#include "fil/nvm/pal/Latency.h"
#include "fil/nvm/pal/LatencyMLC.h"
#include "fil/nvm/pal/LatencySLC.h"
#include "fil/nvm/pal/LatencyTLC.h"
#include "fil/nvm/pal/PAL2.h"
#include "fil/nvm/pal/PALStatistics.h"
#include "util/algorithm.hh"
#include "util/path.hh"

#define FLUSH_PERIOD 100000000000ull  // 0.1sec
#define FLUSH_RANGE 10000000000ull    // 0.01sec

namespace SimpleSSD::FIL::NVM {

PALOLD::PALOLD(ObjectData &o, Event e)
    : AbstractNVM(o, e),
      lastResetTick(0),
      convertObject(o) /* , backingFile(o) */ {
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

  lat->printTiming(object.log, print);

  convertCPDPBP = convertObject.getConvertion();

  stats = new PALStatistics(object.config, lat);
  pal = new PAL2(stats, object.config, lat);

  completeEvent = createEvent([this](uint64_t, uint64_t d) { completion(d); },
                              "FIL::PALOLD::completeEvent");

  // For backingFile
  // planeMultiplier = param->block;
  // dieMultiplier = param->plane * planeMultiplier;
  // wayMultiplier = param->die * dieMultiplier;
  // channelMultiplier = stats->package * wayMultiplier;

  // Create spare data file (platform specific - temp code)
  spareList.reserve(stats->channel * stats->package * param->die *
                    param->plane * param->block * param->die);

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

void PALOLD::submit(Request *req) {
  Complete cplt;

  LPN lpn = req->getLPN();
  cplt.id = req->getTag();
  cplt.ppn = req->getPPN();
  cplt.beginAt = getTick();

  convertCPDPBP(cplt.ppn, cplt.addr);

  /* uint64_t blockID = cplt.addr.Channel * channelMultiplier +
                       cplt.addr.Package * wayMultiplier +
                       cplt.addr.Die * dieMultiplier +
                       cplt.addr.Plane * planeMultiplier + cplt.addr.Block; */

  switch (req->getOpcode()) {
    case Operation::Read:
      cplt.oper = OPER_READ;

      // req->setData(backingFile.read(blockID, cplt.addr.Page));
      // readSpare(cplt.addr, scmd.spare.data(), scmd.spare.size());
      readSpare(cplt.ppn, (uint8_t *)&lpn, sizeof(PPN));

      stat.readCount++;
      break;
    case Operation::Program:
      cplt.oper = OPER_WRITE;

      // backingFile.write(blockID, cplt.addr.Page, req->getData());
      // writeSpare(cplt.addr, scmd.spare.data(), scmd.spare.size());
      writeSpare(cplt.ppn, (uint8_t *)&lpn, sizeof(PPN));

      stat.writeCount++;
      break;
    case Operation::Erase:
      cplt.oper = OPER_ERASE;

      // backingFile.erase(blockID);
      eraseSpare(cplt.ppn);

      stat.eraseCount++;
      break;
    default:
      panic("Operation not supported in PAL.");
      break;
  }

  req->setLPN(lpn);

  ::Command pcmd(cplt.beginAt, cplt.ppn, cplt.oper, param->pageSize);

  debugprint(Log::DebugID::FIL_PALOLD, "%-5s | PPN %" PRIx64 "h",
             OPER_STRINFO2[cplt.oper], cplt.ppn);
  printCPDPBP(cplt.addr, OPER_STRINFO2[cplt.oper]);

  pal->submit(pcmd, cplt.addr);
  cplt.finishedAt = pcmd.finished;

  reschedule(std::move(cplt));
}

void PALOLD::printCPDPBP(::CPDPBP &addr, const char *prefix) {
  debugprint(Log::DebugID::FIL_PALOLD,
             "%-5s | C %5u | W %5u | D %5u | P %5u | B %5u | P %5u", prefix,
             addr.Channel, addr.Package, addr.Die, addr.Plane, addr.Block,
             addr.Page);
}

void PALOLD::reschedule(Complete &&cplt) {
  auto iter = completionQueue.emplace(cplt.id, std::move(cplt));

  panic_if(!iter.second, "Duplicated request ID.");

  scheduleAbs(completeEvent, iter.first->first, iter.first->second.finishedAt);
}

void PALOLD::completion(uint64_t id) {
  auto iter = completionQueue.find(id);

  panic_if(iter == completionQueue.end(), "Unexpected completion.");

  Complete cplt = std::move(iter->second);
  completionQueue.erase(iter);

  debugprint(Log::DebugID::FIL_PALOLD,
             "%-5s | PPN %" PRIx64 "h | %" PRIu64 " - %" PRIu64 " (%" PRIu64
             ")",
             OPER_STRINFO2[cplt.oper], cplt.ppn, cplt.beginAt, cplt.finishedAt,
             cplt.finishedAt - cplt.beginAt);
  printCPDPBP(cplt.addr, OPER_STRINFO2[cplt.oper]);

  scheduleNow(eventRequestCompletion, cplt.id);
}

void PALOLD::readSpare(PPN ppn, uint8_t *data, uint64_t len) {
  panic_if(len > param->spareSize, "Unexpected size of spare data.");

  // Find PPN
  auto iter = spareList.find(ppn);

  if (iter == spareList.end()) {
    memset(data, 0, len);
  }
  else {
    memcpy(data, iter->second.data(), len);
  }
}

void PALOLD::writeSpare(PPN ppn, uint8_t *data, uint64_t len) {
  panic_if(len > param->spareSize, "Unexpected size of spare data.");

  // Find PPN
  auto iter = spareList.find(ppn);

  if (iter == spareList.end()) {
    iter = spareList.emplace(ppn, std::vector<uint8_t>(len, 0)).first;
  }

  memcpy(iter->second.data(), data, len);
}

void PALOLD::eraseSpare(PPN ppn) {
  convertObject.getBlockAlignedPPN(ppn);

  for (uint32_t i = 0; i < param->page; i++) {
    auto iter = spareList.find(ppn);

    if (iter != spareList.end()) {
      spareList.erase(iter);
    }

    convertObject.increasePage(ppn);
  }
}

void PALOLD::getStatList(std::vector<Stat> &list, std::string prefix) noexcept {
  list.emplace_back(prefix + "energy.read",
                    "Consumed energy by NAND read operation (uJ)");
  list.emplace_back(prefix + "energy.program",
                    "Consumed energy by NAND program operation (uJ)");
  list.emplace_back(prefix + "energy.erase",
                    "Consumed energy by NAND erase operation (uJ)");
  list.emplace_back(prefix + "energy.total",
                    "Total consumed energy by NAND (uJ)");
  list.emplace_back(prefix + "power", "Average power consumed by NAND (uW)");
  list.emplace_back(prefix + "read.count", "Total read operation count");
  list.emplace_back(prefix + "program.count", "Total program operation count");
  list.emplace_back(prefix + "erase.count", "Total erase operation count");
  list.emplace_back(prefix + "read.bytes", "Total read operation bytes");
  list.emplace_back(prefix + "program.bytes", "Total program operation bytes");
  list.emplace_back(prefix + "erase.bytes", "Total erase operation bytes");
  list.emplace_back(prefix + "read.time.dma0.wait",
                    "Average dma0 wait time of read");
  list.emplace_back(prefix + "read.time.dma0", "Average dma0 time of read");
  list.emplace_back(prefix + "read.time.mem",
                    "Average memory operation time of read");
  list.emplace_back(prefix + "read.time.dma1.wait",
                    "Average dma1 wait time of read");
  list.emplace_back(prefix + "read.time.dma1", "Average dma1 time of read");
  list.emplace_back(prefix + "read.time.total", "Average time of read");
  list.emplace_back(prefix + "program.time.dma0.wait",
                    "Average dma0 wait time of program");
  list.emplace_back(prefix + "program.time.dma0",
                    "Average dma0 time of program");
  list.emplace_back(prefix + "program.time.mem",
                    "Average memory operation time of program");
  list.emplace_back(prefix + "program.time.dma1.wait",
                    "Average dma1 wait time of program");
  list.emplace_back(prefix + "program.time.dma1",
                    "Average dma1 time of program");
  list.emplace_back(prefix + "program.time.total", "Average time of program");
  list.emplace_back(prefix + "erase.time.dma0.wait",
                    "Average dma0 wait time of erase");
  list.emplace_back(prefix + "erase.time.dma0", "Average dma0 time of erase");
  list.emplace_back(prefix + "erase.time.mem",
                    "Average memory operation time of erase");
  list.emplace_back(prefix + "erase.time.dma1.wait",
                    "Average dma1 wait time of erase");
  list.emplace_back(prefix + "erase.time.dma1", "Average dma1 time of erase");
  list.emplace_back(prefix + "erase.time.total", "Average time of erase");
  list.emplace_back(prefix + "channel.time.active",
                    "Average active time of all channels");
  list.emplace_back(prefix + "die.time.active",
                    "Average active time of all dies");
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

  values.push_back((double)stat.readCount);
  values.push_back((double)stat.writeCount);
  values.push_back((double)stat.eraseCount);

  values.push_back((double)(stat.readCount * param->pageSize));
  values.push_back((double)(stat.writeCount * param->pageSize));
  values.push_back((double)(stat.eraseCount * param->pageSize * param->page));

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
    BACKUP_SCALAR(out, iter.second.id);
    BACKUP_SCALAR(out, iter.second.beginAt);
    BACKUP_SCALAR(out, iter.second.finishedAt);
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
    RESTORE_SCALAR(in, tmp.beginAt);
    RESTORE_SCALAR(in, tmp.finishedAt);

    completionQueue.emplace(tmp.finishedAt, tmp);
  }

  lat->restore(in);
  stats->restore(in);
  pal->restore(in);
}

}  // namespace SimpleSSD::FIL::NVM
