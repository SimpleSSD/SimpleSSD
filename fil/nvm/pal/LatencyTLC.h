// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Gieseo Park <gieseo@camelab.org>
 *         Jie Zhang <jie@camelab.org>
 *         Donghyun Gouk <kukdh1@camelab.org>
 */

#ifndef __LatencyTLC_h__
#define __LatencyTLC_h__

#include "Latency.h"

class LatencyTLC : public Latency {
 private:
  uint64_t read[3];
  uint64_t write[3];
  uint64_t erase;

 public:
  LatencyTLC(SimpleSSD::ConfigReader *);
  ~LatencyTLC();

  void printTiming(SimpleSSD::Log *,
                   void (*)(SimpleSSD::Log *, const char *, ...)) override;

  uint64_t GetLatency(uint32_t, uint8_t, uint8_t) override;
  inline uint8_t GetPageType(uint32_t) override;

  void backup(std::ostream &) const override;
  void restore(std::istream &) override;
};

#endif  //__LatencyTLC_h__
