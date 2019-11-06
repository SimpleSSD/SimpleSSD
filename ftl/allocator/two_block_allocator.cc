// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "ftl/allocator/two_block_allocator.hh"

#include "ftl/mapping/abstract_mapping.hh"

namespace SimpleSSD::FTL::BlockAllocator {

TwoBlockAllocator::TwoBlockAllocator(ObjectData &o, Mapping::AbstractMapping *m)
    : BasicAllocator(o, m), inUseBlockMapSecond(nullptr) {}

TwoBlockAllocator::~TwoBlockAllocator() {
  free(inUseBlockMapSecond);
}

void TwoBlockAllocator::initialize(Parameter *p) {
  BasicAllocator::initialize(p);

  inUseBlockMapSecond = (PPN *)calloc(parallelism, sizeof(PPN));
  lastAllocatedSecond = 0;

  if ((float)parallelism / totalSuperblock * 3.f >= gcThreshold) {
    warn("GC threshold cannot hold minimum blocks. Adjust threshold.");

    gcThreshold = (float)(parallelism + 1) / totalSuperblock * 3.f;
  }
}

CPU::Function TwoBlockAllocator::allocateBlockSecond(PPN &blockUsed) {
  CPU::Function fstat;
  CPU::markFunction(fstat);

  PPN idx = lastAllocatedSecond;

  if (LIKELY(blockUsed != InvalidPPN)) {
    idx = getParallelismFromSPPN(blockUsed);

    panic_if(inUseBlockMapSecond[idx] != blockUsed, "Unexpected block ID.");

    // Insert to full block list
    uint32_t erased = eraseCountList[blockUsed];
    auto iter = fullBlocks[idx].begin();

    for (; iter != fullBlocks[idx].end(); ++iter) {
      if (eraseCountList[*iter] > erased) {
        break;
      }
    }

    fullBlocks[idx].emplace(iter, blockUsed);
    fullBlockCount++;
  }
  else {
    lastAllocatedSecond++;

    if (lastAllocatedSecond == parallelism) {
      lastAllocatedSecond = 0;
    }
  }

  panic_if(freeBlocks[idx].size() == 0, "No more free blocks at ID %" PRIu64,
           idx);

  // Allocate new block
  inUseBlockMapSecond[idx] = freeBlocks[idx].front();
  blockUsed = inUseBlockMapSecond[idx];

  freeBlocks[idx].pop_front();
  freeBlockCount--;

  return fstat;
}

PPN TwoBlockAllocator::getBlockAtSecond(PPN idx) {
  if (idx == InvalidPPN) {
    PPN ppn = inUseBlockMapSecond[lastAllocatedSecond++];

    if (lastAllocatedSecond == parallelism) {
      lastAllocatedSecond = 0;
    }

    return ppn;
  }

  panic_if(idx >= parallelism, "Invalid parallelism index.");

  return inUseBlockMapSecond[idx];
}

bool TwoBlockAllocator::stallRequest() {
  return freeBlockCount <= parallelism * 2;
}

}  // namespace SimpleSSD::FTL::BlockAllocator
