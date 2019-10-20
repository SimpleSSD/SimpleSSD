// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_FIL_FIL_HH__
#define __SIMPLESSD_FIL_FIL_HH__

#include "fil/def.hh"
#include "fil/nvm/abstract_nvm.hh"
#include "hil/command_manager.hh"

namespace SimpleSSD::FIL {

/**
 * \brief FIL (Flash Interface Layer) class
 *
 * Defines abstract layer to the flash interface.
 */
class FIL : public Object {
 private:
  HIL::CommandManager *commandManager;
  NVM::AbstractNVM *pFIL;

 public:
  FIL(ObjectData &, HIL::CommandManager *);
  FIL(const FIL &) = delete;
  FIL(FIL &&) noexcept = default;
  ~FIL();

  FIL &operator=(const FIL &) = delete;
  FIL &operator=(FIL &&) = default;

  /**
   * \brief Submit HIL::Command to FIL
   *
   * This command must have SubCommands with valid ppn field.
   *
   * \param[in] tag HIL::Command tag
   */
  void submit(uint64_t tag);

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::FIL

#endif
