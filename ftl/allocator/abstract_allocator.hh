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
   * \brief Get block metadata at index
   *
   * \param psbn Physical Superblock Number.
   * \return reference to block metadata.
   */
  virtual BlockMetadata &getBlockMetadata(const PSBN &psbn) noexcept = 0;

  /**
   * \brief Get memory address of block metadata
   *
   * \param psbn Physical Superblock Number.
   * \return uint64_t Memory address of block metadata
   */
  virtual uint64_t getMemoryAddressOfBlockMetadata(
      const PSBN &psbn) noexcept = 0;

  /**
   * \brief Allocate new free block
   *
   * Return new freeblock at parallelism index of provided physical page
   * address. Return next freeblock if ppn is invalid.
   *
   * \param[in] psbn  Physical Superblock Number.
   * \return Return CPU firmware execution information.
   */
  virtual CPU::Function allocateFreeBlock(PSBN &psbn) = 0;

  /**
   * \brief Get previously allocated free block
   *
   * Return currently allocated free block at parallelism index of provided
   * physical page address.
   *
   * \param[in] pidx  Parallelism Index
   */
  virtual PSBN getFreeBlockAt(uint32_t psbn) noexcept = 0;

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
   * \brief Check Foreground GC trigger threshold.
   *
   * \return True if Foreground GC should be invoked.
   */
  virtual bool checkForegroundGCThreshold() noexcept = 0;

  /**
   * \brief Check Background GC trigger threshold.
   *
   * \return True if Background GC should be invoked.
   */
  virtual bool checkBackgroundGCThreshold() noexcept = 0;

  /**
   * \brief Select block to erase
   *
   * Return physical block address to erase.
   *
   * \param[in] ctx   CopyContext
   * \param[in] eid   Callback event
   * \param[in] data  Event data
   */
  virtual void getVictimBlocks(CopyContext &ctx, Event eid, uint64_t data) = 0;

  /**
   * \brief Mark block as erased
   *
   * \param[in] psbn  Physical Superblock Number
   * \param[in] eid   Callback event
   * \param[in] data  Event data
   */
  virtual void reclaimBlocks(PSBN psbn, Event eid, uint64_t data) = 0;

  /**
   * \brief Get physical page status (Only for filling phase)
   *
   * Return # of valid pages and # of invalid pages in underlying NAND flash.
   *
   * \param[out] valid    Return # of valid physical (super)pages
   * \param[out] invalid  Return # of invalid physical (super)pages
   */
  virtual void getPageStatistics(uint64_t &valid,
                                 uint64_t &invalid) noexcept = 0;
};

}  // namespace SimpleSSD::FTL::BlockAllocator

#endif
