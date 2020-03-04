// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_FTL_BASE_ABSTRACT_FTL_HH__
#define __SIMPLESSD_FTL_BASE_ABSTRACT_FTL_HH__

#include "fil/fil.hh"
#include "ftl/allocator/abstract_allocator.hh"
#include "ftl/def.hh"
#include "ftl/mapping/abstract_mapping.hh"
#include "sim/object.hh"

namespace SimpleSSD::FTL {

class FTL;

class AbstractFTL : public Object {
 private:
  FTL *pFTL;

 protected:
  FIL::FIL *pFIL;

  Mapping::AbstractMapping *pMapper;
  BlockAllocator::AbstractAllocator *pAllocator;

  Request *getRequest(uint64_t);
  void completeRequest(Request *);

 public:
  AbstractFTL(ObjectData &, FTL *, FIL::FIL *, Mapping::AbstractMapping *,
              BlockAllocator::AbstractAllocator *);
  virtual ~AbstractFTL() {}

  virtual void initialize() {}

  virtual void read(Request *) = 0;
  virtual void write(Request *) = 0;
  virtual void invalidate(LPN, uint32_t, Event, uint64_t) = 0;
};

}  // namespace SimpleSSD::FTL

#endif
