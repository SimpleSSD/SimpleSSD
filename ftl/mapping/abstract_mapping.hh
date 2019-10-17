// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_FTL_MAPPING_ABSTRACT_MAPPING_HH__
#define __SIMPLESSD_FTL_MAPPING_ABSTRACT_MAPPING_HH__

#include "ftl/def.hh"
#include "sim/object.hh"

namespace SimpleSSD::FTL {

namespace BlockAllocator {

class AbstractAllocator;

}

namespace Mapping {

class AbstractMapping : public Object {
 protected:
  Parameter param;
  BlockAllocator::AbstractAllocator *allocator;

 public:
  AbstractMapping(ObjectData &o) : Object(o), allocator(nullptr) {}
  AbstractMapping(const AbstractMapping &) = delete;
  AbstractMapping(AbstractMapping &&) noexcept = default;
  virtual ~AbstractMapping() {}

  AbstractMapping &operator=(const AbstractMapping &) = delete;
  AbstractMapping &operator=(AbstractMapping &&) = default;

  // For AbstractAllocator

  // For FTL
  virtual void initialize(BlockAllocator::AbstractAllocator *) = 0;
  Parameter *getInfo() { return &param; };

  LPN getPageUsage(LPN, LPN);
};

}  // namespace Mapping

}  // namespace SimpleSSD::FTL

#endif
