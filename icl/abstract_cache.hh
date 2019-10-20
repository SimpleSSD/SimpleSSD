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
  const uint64_t minIO = 512;

  HIL::CommandManager *commandManager;
  FTL::FTL *pFTL;

  uint64_t cacheCommandTag;

  inline uint64_t makeCacheCommandTag() {
    return cacheCommandTag++ | ICL_TAG_PREFIX;
  }

 public:
  AbstractCache(ObjectData &o, HIL::CommandManager *m, FTL::FTL *p)
      : Object(o), commandManager(m), pFTL(p), cacheCommandTag(0) {}
  virtual ~AbstractCache() {}

  virtual void enqueue(uint64_t, uint32_t) = 0;

  virtual void setCache(bool) = 0;
  virtual bool getCache() = 0;
};

}  // namespace SimpleSSD::ICL

#endif
