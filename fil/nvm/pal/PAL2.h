// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Jie Zhang <jie@camelab.org>
 *         Donghyun Gouk <kukdh1@camelab.org>
 *         Junhyeok Jang <jhjang@camelab.org>
 */

#pragma once

#ifndef __PAL2_h__
#define __PAL2_h__

#include "SimpleSSD_types.h"

#include "Latency.h"
#include "PALStatistics.h"
#include "fil/fil.hh"

#include "PAL2_TimeSlot.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <fstream>
#include <iostream>
#include <string>
using namespace std;

class PALStatistics;

class PAL2  // let's not inherit PAL1
{
 public:
  PAL2(PALStatistics *statistics, SimpleSSD::ConfigReader *c, Latency *l);
  ~PAL2();

  uint32_t channel;
  uint32_t package;
  SimpleSSD::FIL::Config::NANDStructure *pParam;
  Latency *lat;
  PALStatistics *stats;  // statistics of PAL2, not created by itself

  std::list<TimeSlot> MergedTimeSlots;  // for gathering busy time

  uint64_t totalDie;

  std::map<uint64_t, uint64_t> OpTimeStamp[3];

  std::map<uint64_t, std::map<uint64_t, uint64_t> *> *ChFreeSlots;
  uint64_t *ChStartPoint;  // record the start point of rightmost free slot
  std::map<uint64_t, std::map<uint64_t, uint64_t> *> *DieFreeSlots;
  uint64_t *DieStartPoint;

  void submit(::Command &cmd, CPDPBP &addr);
  void TimelineScheduling(::Command &req, CPDPBP &reqCPD);
  void FlushTimeSlots(uint64_t currentTick);
  void FlushOpTimeStamp();
  void FlushATimeSlotBusyTime(std::list<TimeSlot> &tgtTimeSlot,
                              uint64_t currentTick, uint64_t *TimeSum);

  // you can insert a tickLen
  // TimeSlot after Returned
  // TimeSlot.
  std::list<TimeSlot>::iterator FindFreeTime(std::list<TimeSlot> &tgtTimeSlot,
                                             uint64_t tickLen,
                                             uint64_t tickFrom);

  // Jie: return: FreeSlot is found?
  bool FindFreeTime(
      std::map<uint64_t, std::map<uint64_t, uint64_t> *> &tgtFreeSlot,
      uint64_t tickLen, uint64_t tickFrom, uint64_t &startTick,
      bool &conflicts);
  void InsertFreeSlot(
      std::map<uint64_t, std::map<uint64_t, uint64_t> *> &tgtFreeSlot,
      uint64_t tickLen, uint64_t tickFrom, uint64_t startTick,
      uint64_t &startPoint, bool split);
  void AddFreeSlot(
      std::map<uint64_t, std::map<uint64_t, uint64_t> *> &tgtFreeSlot,
      uint64_t tickLen, uint64_t tickFrom);
  void FlushFreeSlots(uint64_t currentTick);
  void FlushAFreeSlot(
      std::map<uint64_t, std::map<uint64_t, uint64_t> *> &tgtFreeSlot,
      uint64_t currentTick);

  // PPN Conversion related //ToDo: Shifted-Mode is also required for better
  // performance.
  uint32_t RearrangedSizes[7];
  uint8_t AddrRemap[6];
  uint32_t CPDPBPtoDieIdx(CPDPBP *pCPDPBP);
  void printCPDPBP(CPDPBP *pCPDPBP);

  void backup(std::ostream &) const;
  void restore(std::istream &);
};

#endif
