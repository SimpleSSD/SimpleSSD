// SPDX-License-Identifier: LGPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_FTL_WEAR_LEVELING_ABSTRACT_WEAR_LEVELING_HH__
#define __SIMPLESSD_FTL_WEAR_LEVELING_ABSTRACT_WEAR_LEVELING_HH__

#include <random>

#include "fil/fil.hh"
#include "ftl/allocator/victim_selection.hh"
#include "ftl/background_manager/abstract_background_job.hh"
#include "ftl/def.hh"

namespace SimpleSSD::FTL::WearLeveling {

enum class State : uint32_t {
  Idle,

  Foreground,
  Background,
};

class AbstractWearLeveling : public AbstractBlockCopyJob {
 protected:
  State state;

  Event eventEraseCallback;
  virtual void blockEraseCallback(uint64_t now, const PSBN &erased);

 public:
  AbstractWearLeveling(ObjectData &, FTLObjectData &, FIL::FIL *);
  virtual ~AbstractWearLeveling();

  void initialize() override;
  bool isRunning() override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::FTL::WearLeveling

#endif
