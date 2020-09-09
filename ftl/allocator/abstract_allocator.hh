// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 *         Junhyeok Jang <jhjang@camelab.org>
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
  Parameter *param;
  Mapping::AbstractMapping *pMapper;

 public:
  AbstractAllocator(ObjectData &o, Mapping::AbstractMapping *m)
      : Object(o), pMapper(m) {}
  virtual ~AbstractAllocator() {}

  /* Functions for AbstractMapping */

  /**
   * \brief Allocate new free block
   *
   * Return new freeblock at parallelism index of provided physical page
   * address. Return next freeblock if ppn is invalid.
   *
   * \param[in] ppn Physical page address.
   * \param[in] np  Number of pages in mapping granularity.
   * \return Return CPU firmware execution information.
   */
  virtual CPU::Function allocateBlock(PPN &ppn, uint64_t np = 1) = 0;

  /**
   * \brief Get previously allocated free block
   *
   * Return currently allocated free block at parallelism index of provided
   * physical page address.
   *
   * \param[in] ppn Physical page address.
   * \param[in] np  Number of pages in mapping granularity.
   */
  virtual PPN getBlockAt(PPN ppn, uint64_t np = 1) = 0;

  /* Functions for AbstractFTL */

  /**
   * \brief Block allocator initialization function
   *
   * Immediately call AbstractAllocator::initialize() when you override this
   * function.
   * \code{.cc}
   * void YOUR_ALLOCATOR_CLASS::initialize(Parameter *p) {
   *   AbstractAllocator::initialize(p);
   *
   *   // Your initialization code here.
   * }
   * \endcode
   */
  virtual void initialize(Parameter *p) { param = p; };

  /**
   * Check GC trigger threshold.
   *
   * \return True if GC should be invoked.
   */
  virtual bool checkGCThreshold() = 0;

  /**
   * Check if there is a free block.
   * If not, GC must be invoked now.
   *
   * \return True if there is a free block
   */
  virtual bool checkFreeBlockExist() = 0;

  /**
   * \brief Select block to erase
   *
   * Return physical block address to erase. This function may return multiple
   * blocks.
   *
   * fatal: Operation not defined yet.
   */
  virtual void getVictimBlocks(std::vector<PPN> &, Event) = 0;

  /**
   * \brief Mark block as erased
   *
   * fatal: Operation not defined yet.
   */
  virtual void reclaimBlocks(PPN, Event) = 0;

  //! Get parallelism index from physical page address.
  inline PPN getParallelismFromPPN(PPN ppn) { return ppn % param->parallelism; }
};

}  // namespace BlockAllocator

}  // namespace SimpleSSD::FTL

#endif
