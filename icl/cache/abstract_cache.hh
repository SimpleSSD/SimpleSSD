// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_ICL_CACHE_ABSTRACT_CACHE_HH__
#define __SIMPLESSD_ICL_CACHE_ABSTRACT_CACHE_HH__

#include "icl/icl.hh"
#include "sim/object.hh"

namespace SimpleSSD::ICL {

class AbstractManager;

class AbstractCache : public Object {
 protected:
  AbstractManager *manager;

 public:
  AbstractCache(ObjectData &o, AbstractManager *m) : Object(o), manager(m) {}
  virtual ~AbstractCache() {}

  virtual CPU::Function lookup(LPN, bool, bool &) = 0;
  virtual CPU::Function allocate(LPN, uint64_t) = 0;
  virtual CPU::Function flush(LPN, uint32_t) = 0;
  virtual CPU::Function erase(LPN, uint32_t) = 0;
  virtual void dmaDone(LPN) = 0;
  virtual void drainDone() = 0;
};

}  // namespace SimpleSSD::ICL

#endif
