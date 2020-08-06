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

namespace Cache {

class AbstractCache;

}

namespace Manager {

struct FlushContext {
  LPN lpn;           // Logical address of request
  uint64_t address;  // Physical address of internal DRAM

  uint32_t offset;  // Offset in page
  uint32_t length;  // Length in page

  uint8_t *buffer;     // Data (for simulation)
  uint64_t flushedAt;  // For logging

  FlushContext(LPN l, uint64_t a)
      : lpn(l), address(a), offset(0), length(0), buffer(nullptr) {}

  static bool compare(FlushContext &a, FlushContext &b) {
    return a.lpn < b.lpn;
  }
};

class SequentialDetector {
 protected:
  bool enabled;

  uint32_t pageSize;

 public:
  SequentialDetector(uint32_t p) : enabled(false), pageSize(p) {}
  virtual ~SequentialDetector() {}

  inline bool isEnabled() { return enabled; }

  virtual void submitSubRequest(HIL::SubRequest *) = 0;

  virtual void createCheckpoint(std::ostream &out) const noexcept {
    BACKUP_SCALAR(out, enabled);
  }

  virtual void restoreCheckpoint(std::istream &in, ObjectData &) noexcept {
    RESTORE_SCALAR(in, enabled);
  }
};

class AbstractManager : public Object {
 protected:
  ICL *pICL;

  FTL::FTL *pFTL;
  Cache::AbstractCache *cache;

  Event eventICLCompletion;

 public:
  AbstractManager(ObjectData &o, ICL *p, FTL::FTL *f)
      : Object(o),
        pICL(p),
        pFTL(f),
        cache(nullptr),
        eventICLCompletion(InvalidEventID) {}
  virtual ~AbstractManager() {}

  inline HIL::SubRequest *getSubRequest(uint64_t tag) {
    return pICL->getSubRequest(tag);
  }

  void setCallbackFunction(Event e) { eventICLCompletion = e; }
  virtual void initialize(Cache::AbstractCache *ac) { cache = ac; }

  /* Interface for ICL::ICL */

  //! Submit read request
  virtual void read(HIL::SubRequest *) = 0;

  //! Submit write request
  virtual void write(HIL::SubRequest *) = 0;

  //! Submit flush request
  virtual void flush(HIL::SubRequest *) = 0;

  //! Submit trim/format request (erase data in cache)
  virtual void erase(HIL::SubRequest *) = 0;

  //! Called by ICL when DMA is completed (for releasing cacheline)
  virtual void dmaDone(HIL::SubRequest *) = 0;

  /* Interface for ICL::AbstractCache */

  /**
   * \brief Handler for cache lookup
   *
   * Called when cacheline lockup has been completed
   *
   * \param[in] tag Tag of SubRequest
   */
  virtual void lookupDone(uint64_t tag) = 0;

  //! Completion handler for other cache jobs
  virtual void cacheDone(uint64_t tag) = 0;

  //! Cache write-back requester
  virtual void drain(std::vector<FlushContext> &) = 0;
};

}  // namespace Manager

}  // namespace SimpleSSD::ICL

#endif
