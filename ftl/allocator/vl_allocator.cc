// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "ftl/allocator/vl_allocator.hh"

#include "ftl/base/abstract_ftl.hh"
#include "ftl/mapping/abstract_mapping.hh"

namespace SimpleSSD::FTL::BlockAllocator {

VLAllocator::VLAllocator(ObjectData &o, Mapping::AbstractMapping *m)
    : TwoBlockAllocator(o, m), inUseBlockMapLPN(nullptr) {}

VLAllocator::~VLAllocator() {
  free(inUseBlockMapLPN);
}

void VLAllocator::initialize(Parameter *p) {
  TwoBlockAllocator::initialize(p);

  // Allocate LPN mapping
  inUseBlockMapLPN = (LPN *)calloc(parallelism, sizeof(LPN));

  // Fill with InvalidLPN
  for (uint64_t i = 0; i < parallelism; i++) {
    inUseBlockMapLPN[i] = InvalidLPN;
  }
}

// SLPN, SPPN
CPU::Function VLAllocator::allocatePartialBlock(LPN lpn, PPN &ppn) {
  CPU::Function fstat;
  PPN idx = lastAllocatedSecond;

  // Call allocateBlockAtSecond
  fstat += TwoBlockAllocator::allocateBlockSecond(ppn);

  // Find last allocated index
  if (ppn != InvalidPPN) {
    idx = getParallelismFromSPPN(ppn);
  }

  // Record LPN
  inUseBlockMapLPN[idx] = lpn;

  return std::move(fstat);
}

// SLPN, SPPN
PPN VLAllocator::getPartialBlock(LPN lpn, PPN ppn) {
  // If we previously used block with specific lpn?
  uint64_t idx = lpn == InvalidLPN ? parallelism : 0;

  for (; idx < parallelism; idx++) {
    if (inUseBlockMapLPN[idx] == lpn) {
      break;
    }
  }

  if (idx == parallelism) {
    // No, just call getBlockAtSecond
    PPN ret = TwoBlockAllocator::getBlockAtSecond(ppn);

    // We need to store inUseBlockMapLPN with lastAllocatedSecond
    if (lastAllocatedSecond == 0) {
      inUseBlockMapLPN[parallelism - 1] = lpn;
    }
    else {
      inUseBlockMapLPN[lastAllocatedSecond - 1] = lpn;
    }

    return ret;
  }
  else {
    // Yes, return previous one
    return inUseBlockMapSecond[idx];
  }
}

}  // namespace SimpleSSD::FTL::BlockAllocator
