// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_ICL_ABSTRACT_CACHE_HH__
#define __SIMPLESSD_ICL_ABSTRACT_CACHE_HH__

#include "sim/object.hh"
#include "icl/icl.hh"

namespace SimpleSSD::ICL {

class AbstractCache : public Object {
 protected:
  ICL *parent;

 public:
  AbstractCache(ObjectData &o, ICL *p) : Object(o), parent(p) {}
  virtual ~AbstractCache() {}

  virtual void enqueue(Request &) = 0;
};

}  // namespace ICL

#endif
