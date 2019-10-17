// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_FIL_ABSTRACT_NVM_HH__
#define __SIMPLESSD_FIL_ABSTRACT_NVM_HH__

#include <cinttypes>

#include "fil/fil.hh"

namespace SimpleSSD::FIL {

class AbstractNVM : public Object {
 public:
  AbstractNVM(ObjectData &o) : Object(o) {}
  virtual ~AbstractNVM() {}

  virtual void enqueue(Request &&) = 0;
};

}  // namespace SimpleSSD::FIL

#endif
