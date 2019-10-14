// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Gieseo Park <gieseo@camelab.org>
 *         Jie Zhang <jie@camelab.org>
 *         Donghyun Gouk <kukdh1@camelab.org>
 */

#ifndef __LatencyMLC_h__
#define __LatencyMLC_h__

#include "Latency.h"

class LatencyMLC : public Latency {
 private:
  uint64_t read[2];
  uint64_t write[2];
  uint64_t erase;

 public:
  LatencyMLC(SimpleSSD::ConfigReader *);
  ~LatencyMLC();

  uint64_t GetLatency(uint32_t, uint8_t, uint8_t) override;
  inline uint8_t GetPageType(uint32_t) override;
};

#endif  //__LatencyMLC_h__
