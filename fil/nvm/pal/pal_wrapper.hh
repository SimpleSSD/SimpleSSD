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
#include <map>
#include <vector>

#include "SimpleSSD_types.h"
#include "fil/common/file.hh"
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

  uint64_t lastResetTick;

  struct {
    uint64_t readCount;
    uint64_t writeCount;
    uint64_t eraseCount;
  } stat;

  struct Complete {
    uint64_t id;

    PPN ppn;

    uint64_t beginAt;
    uint64_t finishedAt;

    ::CPDPBP addr;
    PAL_OPERATION oper;
  };

  Event completeEvent;
  std::unordered_map<uint64_t, Complete> completionQueue;

  Convert convertObject;
  ConvertFunction convertCPDPBP;

  // NANDBackingFile backingFile;
  // TODO: REVISE THIS WITH BACKING FILE
  std::unordered_map<PPN, std::vector<uint8_t>> spareList;

  void readSpare(PPN, uint8_t *, uint64_t);
  void eraseSpare(PPN);

  // uint64_t channelMultiplier;
  // uint64_t wayMultiplier;
  // uint64_t dieMultiplier;
  // uint64_t planeMultiplier;

  void printCPDPBP(::CPDPBP &, const char *);
  void reschedule(Complete &&);
  void completion(uint64_t);

 public:
  PALOLD(ObjectData &, Event);
  ~PALOLD();

  void submit(Request *req) override;
  void writeSpare(PPN, uint8_t *, uint64_t) override;

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::FIL::NVM

#endif
