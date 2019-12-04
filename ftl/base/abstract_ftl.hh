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

class AbstractFTL : public Object {
 protected:
  FIL::FIL *pFIL;

  Mapping::AbstractMapping *pMapper;
  BlockAllocator::AbstractAllocator *pAllocator;

 public:
  AbstractFTL(ObjectData &o, FIL::FIL *f, Mapping::AbstractMapping *m,
              BlockAllocator::AbstractAllocator *a)
      : Object(o), pFIL(f), pMapper(m), pAllocator(a) {}
  virtual ~AbstractFTL() {}

  virtual void initialize() {}

  virtual void submit(SubRequest *) = 0;
  virtual bool isGC() = 0;
  virtual uint8_t isFormat() = 0;
};

}  // namespace SimpleSSD::FTL

#endif
