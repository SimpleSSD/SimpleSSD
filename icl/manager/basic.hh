// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_ICL_MANAGER_BASIC_HH__
#define __SIMPLESSD_ICL_MANAGER_BASIC_HH__

#include <unordered_map>

#include "icl/manager/abstract_manager.hh"

namespace SimpleSSD::ICL {

class BasicCache : public AbstractManager {
 protected:
  std::unordered_map<uint64_t, SubRequest *> requestQueue;

  Event eventDone;
  Event eventDrainDone;

  Event eventLookupDone;
  void lookupDone(uint64_t, uint64_t);


 public:
  BasicCache(ObjectData &, ICL::ICL *, FTL::FTL *);
  ~BasicCache();

  void read(SubRequest *) override;
  void write(SubRequest *) override;
  void flush(SubRequest *) override;
  void erase(SubRequest *) override;
  void dmaDone(SubRequest *) override;

  void allocateDone(bool, uint64_t) override;
  void flushDone(uint64_t) override;
  void drain(std::vector<FlushContext> &) override;

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::ICL

#endif
