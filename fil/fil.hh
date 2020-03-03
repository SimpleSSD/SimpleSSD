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
#include "fil/scheduler/abstract_scheduler.hh"

namespace SimpleSSD::FIL {

/**
 * \brief FIL (Flash Interface Layer) class
 *
 * Defines abstract layer to the flash interface.
 */
class FIL : public Object {
 private:
  NVM::AbstractNVM *pNVM;
  Scheduler::AbstractScheduler *pScheduler;

  uint64_t requestCounter;
  std::unordered_map<uint64_t, Request> requestQueue;

  void submit(Operation, Request &&);

  Event eventCompletion;
  void completion(uint64_t, uint64_t);

 public:
  FIL(ObjectData &);
  ~FIL();

  /**
   * \brief Read underlying NVM
   *
   * \param[in] req Request object
   */
  void read(Request &&req);

  /**
   * \brief Program/Write underlying NVM
   *
   * \param[in] req Request object
   */
  void program(Request &&req);

  /**
   * \brief Erase underlying NVM
   *
   * \param[in] req Request object
   */
  void erase(Request &&req);

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::FIL

#endif
