// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_ICL_MANAGER_ABSTRACT_MANAGER_HH__
#define __SIMPLESSD_ICL_MANAGER_ABSTRACT_MANAGER_HH__

#include "icl/icl.hh"
#include "sim/object.hh"

namespace SimpleSSD::ICL {

class AbstractCache;

struct FlushContext {
  LPN lpn;           // Logical address of request
  uint64_t address;  // Physical address of internal DRAM
  uint8_t *buffer;   // Data (for simulation)
};

class AbstractManager : public Object {
 protected:
  FTL::FTL *pFTL;
  AbstractCache *cache;

  Event eventICLCompletion;

 public:
  AbstractManager(ObjectData &o, FTL::FTL *p)
      : Object(o),
        pFTL(p),
        cache(nullptr),
        eventICLCompletion(InvalidEventID) {}
  virtual ~AbstractManager() {}

  void setCallbackFunction(Event e) { eventICLCompletion = e; }
  virtual void initialize(AbstractCache *ac) { cache = ac; }

  /* Interface for ICL::ICL */
  virtual void read(SubRequest *) = 0;
  virtual void write(SubRequest *) = 0;
  virtual void flush(SubRequest *) = 0;
  virtual void erase(SubRequest *) = 0;
  virtual void dmaDone(SubRequest *) = 0;

  /* Interface for ICL::AbstractCache */
  virtual void drain(std::vector<FlushContext> &) = 0;
};

}  // namespace SimpleSSD::ICL

#endif
