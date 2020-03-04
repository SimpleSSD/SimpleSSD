// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_FTL_FTL_HH__
#define __SIMPLESSD_FTL_FTL_HH__

#include <deque>

#include "ftl/base/abstract_ftl.hh"
#include "ftl/base/basic_ftl.hh"
#include "ftl/def.hh"
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

  Mapping::AbstractMapping *pMapper;
  BlockAllocator::AbstractAllocator *pAllocator;
  AbstractFTL *pFTL;

  uint64_t requestCounter;

  std::unordered_map<uint64_t, Request> requestQueue;

  Request *insertRequest(Request &&);

 public:
  FTL(ObjectData &);
  ~FTL();

  Parameter *getInfo();

  LPN getPageUsage(LPN, LPN);
  Request *getRequest(uint64_t);
  void completeRequest(Request *);

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
