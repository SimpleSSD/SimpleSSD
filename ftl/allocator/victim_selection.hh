// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_FTL_ALLOCATOR_VICTIM_SELECTION_HH__
#define __SIMPLESSD_FTL_ALLOCATOR_VICTIM_SELECTION_HH__

#include "ftl/object.hh"
#include "sim/object.hh"

namespace SimpleSSD::FTL::BlockAllocator {

class AbstractAllocator;

class AbstractVictimSelection {
 protected:
  FTLObjectData &ftlobject;

 public:
  AbstractVictimSelection(FTLObjectData &);
  virtual ~AbstractVictimSelection();

  /**
   * \brief Select victim block
   *
   * \param idx   Parallelism index.
   * \param psbn  Physical superblock number.
   * \return Firmware execution information.
   */
  virtual CPU::Function getVictim(uint32_t idx, PSBN &psbn) noexcept = 0;
};

}  // namespace SimpleSSD::FTL::BlockAllocator

#endif
