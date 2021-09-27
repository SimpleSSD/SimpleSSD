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

namespace Manager {

class AbstractManager;

}

namespace Cache {

class AbstractCache : public Object {
 public:
  static const uint64_t minIO = 512;

 protected:
  const uint32_t sectorsInPage;

  uint32_t pagesToEvict;

  Manager::AbstractManager *manager;
  const FTL::Parameter *parameter;

  HIL::SubRequest *getSubRequest(uint64_t);

  inline void updateSkip(Bitset &bitset, HIL::SubRequest *req) {
    uint32_t skipFront = req->getSkipFront();
    uint32_t skipEnd = req->getSkipEnd();
    uint32_t skipFrontBit = skipFront / minIO;
    uint32_t skipEndBit = skipEnd / minIO;

    panic_if(skipFront % minIO || skipEnd % minIO,
             "Skip bytes are not aligned to sector size.");

    skipEndBit = sectorsInPage - skipEndBit;

    for (; skipFrontBit < skipEndBit; skipFrontBit++) {
      bitset.set(skipFrontBit);
    }
  }

 public:
  AbstractCache(ObjectData &, Manager::AbstractManager *,
                const FTL::Parameter *);
  virtual ~AbstractCache();

  /**
   * \brief Lookup cache
   *
   * Set sreq->setAllocate() when cache needs new cacheline for current
   * subrequest. Set sreq->setMiss() when cache miss. Allocate and miss are
   * separated because cacheline may contains partial content only.
   * Call manager->lookupDone when completed.
   *
   * \param[in] sreq    Current subrequest
   */
  virtual void lookup(HIL::SubRequest *sreq) = 0;

  /**
   * \brief Flush cacheline
   *
   * Flush cacheline in [offset, offset + length) range. Call manager->cacheDone
   * when completed. Use manager->drain for data write-back.
   *
   * \param[in] sreq  Current subrequest
   */
  virtual void flush(HIL::SubRequest *sreq) = 0;

  /**
   * \brief Erase cacheline
   *
   * Erase (invalidate) cacheline in [offset, offset + length) range. Call
   * manager->cacheDone when completed.
   *
   * \param[in] sreq  Current subrequest
   */
  virtual void erase(HIL::SubRequest *sreq) = 0;

  /**
   * \brief Allocate cacheline in cache
   *
   * This function will be called when sreq->setAllocate() in lookup function.
   * Allocate empty cacheline here and set metadata properly. Call
   * manager->cacheDone when completed. Use manager->drain for data write-back.
   *
   * \param[in] sreq  Current subrequest
   */
  virtual void allocate(HIL::SubRequest *sreq) = 0;

  /**
   * \brief DMA done callback
   *
   * Called when DMA operation (Host <-> DRAM) has been completed.
   *
   * \param[in] sreq  Current subrequest
   */
  virtual void dmaDone(HIL::SubRequest *sreq) = 0;

  /**
   * \brief Drain done callback
   *
   * Called when DMA operation (DRAM <-> NVM) has been completed, requested by
   * ICL::Manger::AbstracterManager::drain() function.
   *
   * \param[in] lpn   LPN address than has been completed
   * \param[in] tag   Drain tag of completed drain request
   * \param[in] drain True when drain (write)
   */
  virtual void drainDone(LPN lpn, uint64_t tag) = 0;

  /**
   * \brief NVM done callback
   *
   * Called when DMA operation (DRAM <-> NVM) has been completed. This function
   * will be called when read operation has been completed.
   *
   * \param[in] lpn   LPN address that has been completed
   * \param[in] tag   HIL::SubRequest tag of completed request
   */
  virtual void nvmDone(LPN, uint64_t tag) = 0;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace Cache

}  // namespace SimpleSSD::ICL

#endif
