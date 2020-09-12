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
#include "ftl/object.hh"
#include "sim/object.hh"

namespace SimpleSSD::FTL::BlockAllocator {

class AbstractAllocator : public Object {
 protected:
  FTLObjectData &ftlobject;

  const Parameter *param;

 public:
  AbstractAllocator(ObjectData &, FTLObjectData &);
  virtual ~AbstractAllocator();

  /* Functions for AbstractMapping */

  /**
   * \brief Allocate new free block
   *
   * Return new freeblock at parallelism index of provided physical page
   * address. Return next freeblock if ppn is invalid.
   *
   * \param[in] psbn  Physical Superblock Number.
   * \return Return CPU firmware execution information.
   */
  virtual CPU::Function allocateBlock(PSBN &psbn) = 0;

  /**
   * \brief Get previously allocated free block
   *
   * Return currently allocated free block at parallelism index of provided
   * physical page address.
   *
   * \param[in] pidx  Parallelism Index
   */
  virtual PSBN getBlockAt(uint32_t psbn) = 0;

  /* Functions for AbstractFTL */

  /**
   * \brief Block allocator initialization function
   *
   * Immediately call AbstractAllocator::initialize() when you override this
   * function.
   * \code{.cc}
   * void YOUR_ALLOCATOR_CLASS::initialize() {
   *   AbstractAllocator::initialize();
   *
   *   // Your initialization code here.
   * }
   * \endcode
   */
  virtual void initialize();

  /**
   * Check Foreground GC trigger threshold.
   *
   * \return True if Foreground GC should be invoked.
   */
  virtual bool checkForegroundGCThreshold() = 0;

  /**
   * Check Background GC trigger threshold.
   *
   * \return True if Background GC should be invoked.
   */
  virtual bool checkBackgroundGCThreshold() = 0;

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
   * \param[in] list  List of Physical Superblock Number
   * \param[in] eid   Callback event
   */
  virtual void getVictimBlocks(std::vector<PSBN> &list, Event eid) = 0;

  /**
   * \brief Mark block as erased
   *
   * \param[in] psbn  Physical Superblock Number
   * \param[in] eid   Callback event
   */
  virtual void reclaimBlocks(PSBN psbn, Event eid) = 0;
};

}  // namespace SimpleSSD::FTL::BlockAllocator

#endif
