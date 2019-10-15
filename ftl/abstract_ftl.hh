// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_FTL_ABSTRACT_FTL_HH__
#define __SIMPLESSD_FTL_ABSTRACT_FTL_HH__

#include <cinttypes>

#include "ftl/ftl.hh"

namespace SimpleSSD::FTL {

struct Status {
  uint64_t totalLogicalPages;
  uint64_t mappedLogicalPages;
  uint64_t freePhysicalBlocks;
};

class AbstractFTL : public Object {
 protected:
  FTL *parent;

  Status status;

 public:
  AbstractFTL(ObjectData &o, FTL *p) : Object(o), parent(p) {}
  virtual ~AbstractFTL() {}

  virtual bool initialize() = 0;

  virtual void enqueue(Request &) = 0;
  virtual Status *getStatus(uint64_t, uint64_t) = 0;
};

}  // namespace SimpleSSD::FTL

#endif
