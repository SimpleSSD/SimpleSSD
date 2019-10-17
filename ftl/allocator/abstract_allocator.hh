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
 protected:
  Mapping::AbstractMapping *mapper;

 public:
  AbstractAllocator(ObjectData &o, Mapping::AbstractMapping *m)
      : Object(o), mapper(m) {}
  AbstractAllocator(const AbstractAllocator &) = delete;
  AbstractAllocator(AbstractAllocator &&) noexcept = default;
  ~AbstractAllocator() {}

  AbstractAllocator &operator=(const AbstractAllocator &) = delete;
  AbstractAllocator &operator=(AbstractAllocator &&) = default;

  // For AbstractMapping

  // For FTL
};

}  // namespace BlockAllocator

}  // namespace SimpleSSD::FTL

#endif
