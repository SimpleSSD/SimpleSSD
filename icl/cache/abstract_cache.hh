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
  static const uint64_t minIO = 512;

  AbstractManager *manager;
  FTL::Parameter *parameter;

 public:
  AbstractCache(ObjectData &o, AbstractManager *m, FTL::Parameter *p)
      : Object(o), manager(m), parameter(p) {}
  virtual ~AbstractCache() {}

  /**
   * \brief Lookup cache
   *
   * Set sreq->setHit() when current subrequest is found on cache.
   * When read, setHit() should be called when cache hit.
   * When write, setHit() should be called when cache hit and cold miss.
   *
   * \param[in] sreq    Current subrequest
   * \param[in] isRead  True when read
   */
  virtual CPU::Function lookup(SubRequest *sreq, bool isRead) = 0;

  /**
   * \brief Allocate cacheline in cache
   *
   * When miss, cache should make place to store new data. Call
   * AbstractManager::drain for data write-back.
   *
   * \param[in] sreq  Current subrequest
   */
  virtual CPU::Function allocate(SubRequest *sreq) = 0;

  /**
   * \brief Flush cacheline
   *
   * Flush cacheline in [offset, offset + length) range.
   *
   * \param[in] sreq  Current subrequest
   */
  virtual CPU::Function flush(SubRequest *sreq) = 0;

  /**
   * \brief Erase cacheline
   *
   * Erase (invalidate) cacheline in [offset, offset + length) range.
   *
   * \param[in] sreq  Current subrequest
   */
  virtual CPU::Function erase(SubRequest *sreq) = 0;

  /**
   * \brief DMA done callback
   *
   * Called when DMA operation (Host <-> DRAM) has been completed.
   *
   * \param[in] lpn LPN address than completed
   */
  virtual void dmaDone(LPN lpn) = 0;

  /**
   * \brief NVM done callback
   *
   * Called when DMA operation (DRAM <-> NVM) has been completed.
   *
   * \param[in] lpn LPN address than completed
   */
  virtual void nvmDone(LPN lpn) = 0;

  /**
   * \brief Drain done callback
   *
   * Called when drain (write-back) operation has been completed.
   */
  virtual void drainDone() = 0;
};

}  // namespace SimpleSSD::ICL

#endif
