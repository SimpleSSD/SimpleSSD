// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_FTL_FTL_HH__
#define __SIMPLESSD_FTL_FTL_HH__

#include <deque>

#include "ftl/base/abstract_ftl.hh"
#include "ftl/base/basic_ftl.hh"
#include "ftl/def.hh"
#include "hil/command_manager.hh"

namespace SimpleSSD::FTL {

/**
 * \brief FTL (Flash Translation Layer) class
 *
 * Defines abstract layer to the flash translation layer.
 */
class FTL : public Object {
 private:
  struct FormatContext {
    Event eid;
    uint64_t data;
  };

  CommandManager *commandManager;
  FIL::FIL *pFIL;

  Mapping::AbstractMapping *pMapper;
  BlockAllocator::AbstractAllocator *pAllocator;
  AbstractFTL *pFTL;

 public:
  FTL(ObjectData &, CommandManager *);
  FTL(const FTL &) = delete;
  FTL(FTL &&) noexcept = default;
  ~FTL();

  FTL &operator=(const FTL &) = delete;
  FTL &operator=(FTL &&) = default;

  void submit(uint64_t);

  Parameter *getInfo();

  LPN getPageUsage(LPN, LPN);
  bool isGC();
  uint8_t isFormat();

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::FTL

#endif
