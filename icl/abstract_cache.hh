// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_ICL_ABSTRACT_CACHE_HH__
#define __SIMPLESSD_ICL_ABSTRACT_CACHE_HH__

#include "icl/icl.hh"
#include "sim/object.hh"

namespace SimpleSSD::ICL {

class AbstractCache : public Object {
 protected:
  const uint32_t minIO = 512;

  FTL::FTL *pFTL;

 public:
  AbstractCache(ObjectData &o, FTL::FTL *p) : Object(o), pFTL(p) {}
  virtual ~AbstractCache() {}

  virtual void submit(SubRequest *) = 0;

  virtual void setCache(bool) = 0;
  virtual bool getCache() = 0;
};

}  // namespace SimpleSSD::ICL

#endif
