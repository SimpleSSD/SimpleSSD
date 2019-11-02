// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_FTL_ALLOCATOR_TWO_BLOCK_ALLOCATOR_HH__
#define __SIMPLESSD_FTL_ALLOCATOR_TWO_BLOCK_ALLOCATOR_HH__

#include <list>
#include <random>

#include "ftl/allocator/basic_allocator.hh"

namespace SimpleSSD::FTL::BlockAllocator {

class TwoBlockAllocator : public BasicAllocator {
 protected:
  PPN lastAllocatedSecond;   // Used for pMapper->initialize
  PPN *inUseBlockMapSecond;  // Allocated free blocks

 public:
  TwoBlockAllocator(ObjectData &, Mapping::AbstractMapping *);
  virtual ~TwoBlockAllocator();

  void initialize(Parameter *) override;

  virtual CPU::Function allocateBlockSecond(PPN &);
  virtual PPN getBlockAtSecond(PPN);

  bool stallRequest() override;
};

}  // namespace SimpleSSD::FTL::BlockAllocator

#endif
