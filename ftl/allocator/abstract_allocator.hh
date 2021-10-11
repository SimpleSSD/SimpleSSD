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

#include "ftl/allocator/victim_selection.hh"
#include "ftl/def.hh"
#include "ftl/object.hh"
#include "sim/object.hh"

namespace SimpleSSD::FTL::BlockAllocator {

class AbstractAllocator : public Object {
 protected:
  FTLObjectData &ftlobject;

  const Parameter *const param;

  std::vector<Event> eventList;

  /**
   * \brief Call registered events
   *
   * This function must be called when reclaimBlock() called.
   */
  void callEvents(const PSBN &);

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
   * \param[inout] psbn     Physical Superblock Number.
   * \param[in]    strategy Strategy of free block selection
   * \return CPU firmware execution information.
   */
  virtual CPU::Function allocateFreeBlock(
      PSBN &psbn,
      AllocationStrategy strategy = AllocationStrategy::LowestEraseCount) = 0;

  /**
   * \brief Get previously allocated free block
   *
   * Return currently allocated free block at parallelism index of provided
   * physical page address.
   *
   * \param[in] pidx  Parallelism Index
   * \param[in] strategy Strategy of free block selection
   * \return Physical superblock number
   */
  virtual PSBN getFreeBlockAt(
      uint32_t pidx, AllocationStrategy strategy =
                         AllocationStrategy::LowestEraseCount) noexcept = 0;

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
   * \brief Get physical page status (Only for filling phase)
   *
   * Return # of valid pages and # of invalid pages in underlying NAND flash.
   *
   * \param[out] valid    Return # of valid physical (super)pages
   * \param[out] invalid  Return # of invalid physical (super)pages
   */
  virtual void getPageStatistics(uint64_t &valid,
                                 uint64_t &invalid) noexcept = 0;

  /* Functions for background jobs */

  /**
   * \brief Select block to erase
   *
   * Return physical block address to erase.
   * If method is null, ctx.blockID must be valid and should be full block.
   *
   * \param[in] ctx     CopyContext
   * \param[in] method  Victim block selection algorithm
   * \param[in] eid     Callback event
   * \param[in] data    Event data
   */
  virtual void getVictimBlock(CopyContext &ctx, AbstractVictimSelection *method,
                              Event eid, uint64_t data) = 0;

  /**
   * \brief Mark block as erased
   *
   * \param[in] psbn  Physical Superblock Number
   * \param[in] eid   Callback event
   * \param[in] data  Event data
   */
  virtual void reclaimBlock(PSBN psbn, Event eid, uint64_t data) = 0;

  /**
   * \brief Register event listener for block erase
   *
   * Event will scheduled immediately when block is reclaimed.
   * Event data will be physical superblock number.
   *
   * \param[in] eid Callback event
   */
  void registerBlockEraseEventListener(Event eid);

  /* Functions for AbstractVictimSelection */

  /**
   * \brief Get full block list at specified parallelism index
   *
   * \param index Parallelism Index.
   * \return List of full blocks.
   */
  virtual std::list<PSBN> &getBlockListAtParallelismIndex(
      uint32_t index) noexcept = 0;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::FTL::BlockAllocator

#endif
