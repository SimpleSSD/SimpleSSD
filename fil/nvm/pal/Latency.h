// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Gieseo Park <gieseo@camelab.org>
 *         Jie Zhang <jie@camelab.org>
 *         Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __Latency_h__
#define __Latency_h__

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <fstream>
#include <iostream>
#include <string>

#include "SimpleSSD_types.h"
using namespace std;

#include "fil/config.hh"
#include "sim/object.hh"

/*==============================
    Latency
==============================*/
class Latency {
 protected:
  SimpleSSD::FIL::Config::NANDTiming *timing;
  SimpleSSD::FIL::Config::NANDPower *power;

  // Calculated DMA parameters
  uint64_t readdma0;
  uint64_t readdma1;
  uint64_t writedma0;
  uint64_t writedma1;
  uint64_t erasedma0;
  uint64_t erasedma1;

  // Calculated power parameters
  uint64_t powerbus;
  uint64_t powerread;
  uint64_t powerwrite;
  uint64_t powererase;
  uint64_t powerstandby;

 public:
  Latency(SimpleSSD::ConfigReader *);
  virtual ~Latency();

  virtual void printTiming(SimpleSSD::Log *,
                           void (*)(SimpleSSD::Log *, const char *, ...)) = 0;

  // Get Latency for PageAddress(L/C/MSBpage), Operation(RWE),
  // BusyFor(Ch.DMA/Mem.Work)
  virtual uint64_t GetLatency(uint32_t, uint8_t, uint8_t) { return 0; };
  virtual inline uint8_t GetPageType(uint32_t) { return PAGE_NUM; };

  // Setup DMA speed and pagesize
  virtual uint64_t GetPower(uint8_t, uint8_t);

  virtual void backup(std::ostream &) const;
  virtual void restore(std::istream &);
};

#endif  //__Latency_h__
