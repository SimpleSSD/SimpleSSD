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
#include "hil/command_manager.hh"
#include "sim/object.hh"

namespace SimpleSSD::FTL {

namespace Mapping {

class AbstractMapping;

}

namespace BlockAllocator {

class AbstractAllocator : public Object {
 protected:
  Parameter *param;
  Mapping::AbstractMapping *pMapper;

 public:
  AbstractAllocator(ObjectData &o, Mapping::AbstractMapping *m)
      : Object(o), pMapper(m) {}
  AbstractAllocator(const AbstractAllocator &) = delete;
  AbstractAllocator(AbstractAllocator &&) noexcept = default;
  ~AbstractAllocator() {}

  AbstractAllocator &operator=(const AbstractAllocator &) = delete;
  AbstractAllocator &operator=(AbstractAllocator &&) = default;

  virtual void initialize(Parameter *p) { param = p; };

  // For AbstractMapping
  virtual CPU::Function allocateBlock(PPN &) = 0;
  virtual PPN getBlockAt(PPN) = 0;

  // For FTL
  virtual bool checkGCThreshold() = 0;
  virtual void getVictimBlocks(std::deque<PPN> &, Event) = 0;
  virtual void reclaimBlocks(std::vector<SubCommand> &, Event) = 0;

  virtual PPN getSuperParallelismIndex(PPN ppn) {
    return (ppn % param->parallelism) / param->superpage;
  }

  virtual inline PPN getParallelismIndex(PPN ppn) {
    return ppn % param->parallelism;
  }
};

}  // namespace BlockAllocator

}  // namespace SimpleSSD::FTL

#endif
