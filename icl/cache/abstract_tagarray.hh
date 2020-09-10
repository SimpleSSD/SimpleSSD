// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_ICL_CACHE_ABSTRACT_TAG_HH__
#define __SIMPLESSD_ICL_CACHE_ABSTRACT_TAG_HH__

#include "icl/cache/abstract_cache.hh"
#include "icl/manager/abstract_manager.hh"

namespace SimpleSSD::ICL::Cache {

struct CacheTag {
  union {
    uint8_t data;
    struct {
      uint8_t valid : 1;       //!< Entry is valid
      uint8_t dirty : 1;       //!< Entry is dirty - should be written back
      uint8_t nvmPending : 1;  //!< Currently in reading/writing to NVM
      uint8_t dmaPending : 1;  //!< Currently in reading/writing to Host
      uint8_t rsvd : 4;
    };
  };

  LPN tag;              //!< LPN address of this cacheline
  uint64_t insertedAt;  //!< Inserted time
  uint64_t accessedAt;  //!< Created time
  Bitset validbits;     //!< Valid sector bits

  CacheTag(uint64_t size)
      : data(0), tag(0), insertedAt(0), accessedAt(0), validbits(size) {}

  void createCheckpoint(std::ostream &out) const noexcept {
    BACKUP_SCALAR(out, data);
    BACKUP_SCALAR(out, tag);
    BACKUP_SCALAR(out, insertedAt);
    BACKUP_SCALAR(out, accessedAt);

    validbits.createCheckpoint(out);
  }

  void restoreCheckpoint(std::istream &in) noexcept {
    RESTORE_SCALAR(in, data);
    RESTORE_SCALAR(in, tag);
    RESTORE_SCALAR(in, insertedAt);
    RESTORE_SCALAR(in, accessedAt);

    validbits.restoreCheckpoint(in);
  }
};

struct WritebackRequest {
  uint64_t tag;       // Tag of flush request
  uint64_t drainTag;  // Last tag of drain FTL request
  bool flush;         // True if tag is valid

  std::unordered_map<LPN, CacheTag *> lpnList;

  WritebackRequest() : tag(0), drainTag(0), flush(false) {}
};

class AbstractTagArray : public Object {
 protected:
  Manager::AbstractManager *manager;

  uint32_t pagesToEvict;
  const uint32_t sectorsInPage;

  inline HIL::SubRequest *getSubRequest(uint64_t tag) {
    return manager->getSubRequest(tag);
  }

  Event eventLookupDone;
  Event eventCacheDone;

 public:
  AbstractTagArray(ObjectData &o, Manager::AbstractManager *m,
                   FTL::Parameter *p)
      : Object(o),
        manager(m),
        pagesToEvict(0),
        sectorsInPage(DIVCEIL(p->pageSize, AbstractCache::minIO)) {}
  virtual ~AbstractTagArray() {}

  //! Initialize
  void initialize(uint32_t p, Event el, Event ed) noexcept {
    pagesToEvict = p;
    eventLookupDone = el;
    eventCacheDone = ed;
  }

  //! Return TagArray size
  virtual uint64_t getArraySize() = 0;

  //! Return memory address of cacheline data
  virtual uint64_t getDataAddress(CacheTag *) = 0;

  //! Return event should be called when lookup is completed
  virtual Event getLookupMemoryEvent() { return eventLookupDone; }

  //! Return event should be called when all tag array has been read
  virtual Event getReadAllMemoryEvent() { return eventCacheDone; }

  //! Return event should be called when allocate is completed
  virtual Event getWriteOneMemoryEvent() { return eventCacheDone; }

  //! Clear all flags of cachelines in range
  virtual CPU::Function erase(LPN slpn, uint32_t nlp) = 0;

  /**
   * \brief Check new cacheline can be allocated
   *
   * When Host DMA or NAND I/O has been completed, pending cacheline allocation
   * should be performed. This function checks pending allocation can be handled
   * by completion of previous operation.
   *
   * \param[in] lpn   LPN that pending DMA operation hs been completed
   * \param[in] sreq  Target HIL::SubRequest should be allocated
   * \return  Return true when cacheline can be allocated
   */
  virtual bool checkAllocatable(LPN lpn, HIL::SubRequest *sreq) = 0;

  /**
   * \brief Check current cacheline has pending DMA/NAND operation
   *
   * If hitted cacheline has pending operation, we need to wait until operation
   * completed.
   *
   * \param[in] ctag  Pointer to cacheline
   * \return  Return truenwhen cacheline has pending operation
   */
  virtual bool checkPending(CacheTag *ctag) {
    return ctag->dmaPending || ctag->nvmPending;
  }

  /**
   * \brief Collect evictable cachelines
   *
   * When there is no cacheline for write, eviction should be performed. This
   * function must collect evictable (valid, dirty and no pending DMA / NAND
   * operations) cachelines and return as vector of Manager::FlushContext.
   *
   * TODO: If we implement NVM Set, this function may require range parameters.
   *
   * \param[in]  lpn    Target LPN should be written to cacheline
   * \param[out] wbreq  Return cachelines can be evicted
   */
  virtual void collectEvictable(LPN lpn, WritebackRequest &wbreq) = 0;

  /**
   * \brief Collect flushable cachelines in range
   *
   * This function collects flushable (valid, dirty and no pending DMa / NAND
   * operations) cachelines within range and return as vector of
   * Manager::FlushContext.
   *
   * \param[in]  slpn   Starting lpn
   * \param[in]  nlp    # of pages
   * \param[out] wbreq  Return cachelines can be flushed
   */
  virtual void collectFlushable(LPN slpn, uint32_t nlp,
                                WritebackRequest &wbreq) = 0;
  /**
   * \brief Get valid cacheline
   *
   * Find cacheline corresponding to provided LPN. If no cacheline exists,
   * return nullptr.
   *
   * \param[in]  lpn  LPN to check
   * \param[out] ctag Pointer to valid cacheline or nullptr
   */
  virtual CPU::Function getValidLine(LPN lpn, CacheTag **ctag) = 0;

  // void getCleanWay(uint32_t, uint32_t &);

  /**
   * \brief Get allocatable (empty or clean)
   *
   * Find empty or clean cachline to allocate new cacheline. If no cacheline is
   * allocatable, return nullptr.
   *
   * \param[in]  lpn  LPN should be allocated
   * \param[out] ctag Pointer to allocated cacheline or nullptr
   */
  virtual CPU::Function getAllocatableLine(LPN lpn, CacheTag **ctag) = 0;

  /**
   * \brief Return string representation of CacheTag
   *
   * This function is only used for logging purpose.
   */
  virtual std::string print(CacheTag *) noexcept = 0;

  //! Return log ID
  virtual Log::DebugID getLogID() noexcept = 0;

  //! For createCheckpoint
  virtual uint64_t getOffset(CacheTag *) const noexcept = 0;

  //! For restoreCheckpoint
  virtual CacheTag *getTag(uint64_t) noexcept = 0;
};

}  // namespace SimpleSSD::ICL::Cache

#endif
