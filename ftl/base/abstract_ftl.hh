// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_FTL_BASE_ABSTRACT_FTL_HH__
#define __SIMPLESSD_FTL_BASE_ABSTRACT_FTL_HH__

#include "fil/fil.hh"
#include "ftl/allocator/abstract_allocator.hh"
#include "ftl/def.hh"
#include "ftl/mapping/abstract_mapping.hh"
#include "sim/object.hh"

namespace SimpleSSD::FTL {

class FTL;

class AbstractFTL : public Object {
 private:
  FTL *pFTL;

 protected:
  FIL::FIL *pFIL;

  Mapping::AbstractMapping *pMapper;
  BlockAllocator::AbstractAllocator *pAllocator;

  void completeRequest(Request *);

 public:
  AbstractFTL(ObjectData &, FTL *, FIL::FIL *, Mapping::AbstractMapping *,
              BlockAllocator::AbstractAllocator *);
  virtual ~AbstractFTL() {}

  Request *getRequest(uint64_t);

  virtual void initialize() {}

  virtual void read(Request *) = 0;
  virtual void write(Request *) = 0;
  virtual void invalidate(Request *) = 0;

  // In initialize phase of mapping, they may want to write spare area
  void writeSpare(PPN ppn, uint8_t *buffer, uint64_t size) {
    pFIL->writeSpare(ppn, buffer, size);
  }

  // Demand paging
  void writeLastPage(PPN sppn, PPN superpage) {
    panic_if(superpage != 1, "Demand paging only works when using pure-page.");

    pFIL->program(FIL::Request(sppn, InvalidEventID, 0));
  }

  void readLastPage(PPN sppn, PPN superpage, Event eid, uint64_t tag) {
    panic_if(superpage != 1, "Demand paging only works when using pure-page.");

    pFIL->read(FIL::Request(sppn, eid, tag));
  }
};

}  // namespace SimpleSSD::FTL

#endif
