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
  ICL::ICL *pICL;

  FTL::FTL *pFTL;
  AbstractCache *cache;

  Event eventICLCompletion;

 public:
  AbstractManager(ObjectData &o, ICL::ICL *p, FTL::FTL *f)
      : Object(o),
        pICL(p),
        pFTL(f),
        cache(nullptr),
        eventICLCompletion(InvalidEventID) {}
  virtual ~AbstractManager() {}

  void setCallbackFunction(Event e) { eventICLCompletion = e; }
  virtual void initialize(AbstractCache *ac) { cache = ac; }

  /* Interface for ICL::ICL */
  //! Submit read request
  virtual void read(SubRequest *) = 0;

  //! Submit write request
  virtual void write(SubRequest *) = 0;

  //! Submit flush request
  virtual void flush(SubRequest *) = 0;

  //! Submit trim/format request (erase data in cache)
  virtual void erase(SubRequest *) = 0;

  //! Called by ICL when DMA is completed (for releasing cacheline)
  virtual void dmaDone(SubRequest *) = 0;

  /* Interface for ICL::AbstractCache */
  /**
   * \brief Handler for cache allocation
   *
   * Called when allocating cacheline for new data has been completed
   *
   * \param[in] read  True when SubRequest(tag) was read
   * \param[in] tag   Tag of SubRequest
   */
  virtual void allocateDone(bool read, uint64_t tag) = 0;

  //! Handler for cache flush
  virtual void flushDone(uint64_t) = 0;

  //! Cache write-back requester
  virtual void drain(std::vector<FlushContext> &) = 0;
};

}  // namespace SimpleSSD::ICL

#endif
