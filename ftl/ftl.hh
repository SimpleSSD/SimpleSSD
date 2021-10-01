// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 *         Junhyeok Jang <jhjang@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_FTL_FTL_HH__
#define __SIMPLESSD_FTL_FTL_HH__

#include <deque>

#include "ftl/def.hh"
#include "ftl/job_manager.hh"
#include "ftl/object.hh"
#include "hil/request.hh"

namespace SimpleSSD::FTL {

/**
 * \brief FTL (Flash Translation Layer) class
 *
 * Defines abstract layer to the flash translation layer.
 */
class FTL : public Object {
 private:
  FIL::FIL *pFIL;

  FTLObjectData ftlobject;
  JobManager jobManager;

  uint64_t requestCounter;

  std::unordered_map<uint64_t, Request> requestQueue;

  Request *insertRequest(Request &&);

 public:
  FTL(ObjectData &);
  ~FTL();

  void initialize();
  const Parameter *getInfo();

  uint64_t getPageUsage(LPN, uint64_t);
  Request *getRequest(uint64_t);
  void completeRequest(Request *);
  inline uint64_t generateFTLTag() { return ++requestCounter; }

  void read(Request &&);
  void write(Request &&);
  void invalidate(Request &&);

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::FTL

#endif
