// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_FIL_PAL_OLD_HH__
#define __SIMPLESSD_FIL_PAL_OLD_HH__

#include <cinttypes>
#include <vector>

#include "SimpleSSD_types.h"
#include "fil/nvm/abstract_nvm.hh"
#include "fil/nvm/pal/convert.hh"

class PAL2;
class PALStatistics;
class Latency;

namespace SimpleSSD::FIL::NVM {

class PALOLD : public AbstractNVM {
 private:
  ::PAL2 *pal;
  ::PALStatistics *stats;
  ::Latency *lat;

  Config::NANDStructure *param;

  Event flushEvent;

  uint64_t lastResetTick;

  struct {
    uint64_t readCount;
    uint64_t writeCount;
    uint64_t eraseCount;
  } stat;

  struct Complete {
    uint64_t id;

    Event eid;
    uint64_t data;

    uint64_t beginAt;
    uint64_t finishedAt;
  };

  Event completeEvent;
  std::list<Complete> completionQueue;

  ConvertFunction convertCPDPBP;

  void printCPDPBP(::CPDPBP &, const char *);
  void reschedule(Complete &&);
  void completion(uint64_t);

 public:
  PALOLD(ObjectData &);
  ~PALOLD();

  void enqueue(Request &&) override;

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::FIL

#endif
