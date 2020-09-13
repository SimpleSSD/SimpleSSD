// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Gieseo Park <gieseo@camelab.org>
 *         Jie Zhang <jie@camelab.org>
 *         Donghyun Gouk <kukdh1@camelab.org>
 */

#ifndef __LatencySLC_h__
#define __LatencySLC_h__

#include "Latency.h"

class LatencySLC : public Latency {
 private:
  uint64_t read;
  uint64_t write;
  uint64_t erase;

 public:
  LatencySLC(SimpleSSD::ConfigReader *);
  ~LatencySLC();

  void printTiming(SimpleSSD::Log *) override;

  uint64_t GetLatency(uint32_t, uint8_t, uint8_t) override;
  inline uint8_t GetPageType(uint32_t) override;

  void backup(std::ostream &) const override;
  void restore(std::istream &) override;
};

#endif  //__LatencyTLC_h__
