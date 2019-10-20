// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_FTL_ALLOCATOR_ABSTRACT_ALLOCATOR_HH__
#define __SIMPLESSD_FTL_ALLOCATOR_ABSTRACT_ALLOCATOR_HH__

#include "ftl/def.hh"
#include "sim/object.hh"

namespace SimpleSSD::FTL {

namespace Mapping {

class AbstractMapping;

}

namespace BlockAllocator {

class AbstractAllocator : public Object {
 public:
  AbstractAllocator(ObjectData &o) : Object(o) {}
  AbstractAllocator(const AbstractAllocator &) = delete;
  AbstractAllocator(AbstractAllocator &&) noexcept = default;
  ~AbstractAllocator() {}

  AbstractAllocator &operator=(const AbstractAllocator &) = delete;
  AbstractAllocator &operator=(AbstractAllocator &&) = default;

  virtual void initialize(Parameter *) = 0;

  // For AbstractMapping
  virtual void allocateBlock(PPN, Event) = 0;

  // For FTL
  virtual void getVictimBlocks(std::deque<PPN> &, Event) = 0;
  virtual void reclaimBlocks(std::deque<PPN> &, Event) = 0;
};

}  // namespace BlockAllocator

}  // namespace SimpleSSD::FTL

#endif
