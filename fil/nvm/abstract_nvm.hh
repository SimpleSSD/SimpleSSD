// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_FIL_NVM_ABSTRACT_NVM_HH__
#define __SIMPLESSD_FIL_NVM_ABSTRACT_NVM_HH__

#include <cinttypes>

#include "fil/request.hh"
#include "ftl/def.hh"
#include "sim/object.hh"

namespace SimpleSSD::FIL::NVM {

class AbstractNVM : public Object {
 protected:
  Event eventRequestCompletion;

 public:
  AbstractNVM(ObjectData &o, Event e) : Object(o), eventRequestCompletion(e) {}
  virtual ~AbstractNVM() {}

  /**
   * \brief Submit command
   *
   * \param[in] req Request object
   */
  virtual void submit(Request *req) = 0;

  /**
   * \brief Write spare data without timing calculation
   *
   * This function should only be used in FTL initialization (warm-up)
   * procedure.
   */
  virtual void writeSpare(PPN, uint8_t *, uint64_t) = 0;
};

}  // namespace SimpleSSD::FIL::NVM

#endif
