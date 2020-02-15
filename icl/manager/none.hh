// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_ICL_MANAGER_NONE_HH__
#define __SIMPLESSD_ICL_MANAGER_NONE_HH__

#include "icl/manager/abstract_manager.hh"

namespace SimpleSSD::ICL {

class NoCache : public AbstractManager {
 public:
  NoCache(ObjectData &o, ICL::ICL *p, FTL::FTL *f) : AbstractManager(o, p, f) {}
  ~NoCache() {}

  void read(HIL::SubRequest *) override {
    // TODO: FTL
  }

  void write(HIL::SubRequest *) override {
    // TODO: FTL
  }

  void flush(HIL::SubRequest *) override {}

  void erase(HIL::SubRequest *) override {
    // TODO: FTL
  }

  void dmaDone(HIL::SubRequest *) override {}

  void allocateDone(bool, uint64_t) override {}
  void flushDone(uint64_t) override {}
  void drain(std::vector<FlushContext> &) override {}

  void getStatList(std::vector<Stat> &, std::string) noexcept override {}
  void getStatValues(std::vector<double> &) noexcept override {}
  void resetStatValues() noexcept override {}

  void createCheckpoint(std::ostream &) const noexcept override {}
  void restoreCheckpoint(std::istream &) noexcept override {}
};

}  // namespace SimpleSSD::ICL

#endif
