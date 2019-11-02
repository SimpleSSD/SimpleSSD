// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_FTL_ALLOCATOR_VL_ALLOCATOR_HH__
#define __SIMPLESSD_FTL_ALLOCATOR_VL_ALLOCATOR_HH__

#include "ftl/allocator/two_block_allocator.hh"

namespace SimpleSSD::FTL::BlockAllocator {

class VLAllocator : public TwoBlockAllocator {
 private:
  LPN *inUseBlockMapLPN;

 public:
  VLAllocator(ObjectData &, Mapping::AbstractMapping *);
  ~VLAllocator();

  void initialize(Parameter *) override;

  CPU::Function allocatePartialBlock(LPN, PPN &);
  PPN getPartialBlock(LPN, PPN);
};

}  // namespace SimpleSSD::FTL::BlockAllocator

#endif
