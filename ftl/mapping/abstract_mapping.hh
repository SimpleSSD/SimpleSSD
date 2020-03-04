// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_FTL_MAPPING_ABSTRACT_MAPPING_HH__
#define __SIMPLESSD_FTL_MAPPING_ABSTRACT_MAPPING_HH__

#include "ftl/def.hh"
#include "hil/request.hh"
#include "sim/object.hh"

namespace SimpleSSD::FTL {

class AbstractFTL;

namespace BlockAllocator {

class AbstractAllocator;

}

namespace Mapping {

struct BlockMetadata {
  PPN blockID;

  uint32_t nextPageToWrite;
  uint16_t clock;  // For cost benefit
  Bitset validPages;

  BlockMetadata() : blockID(InvalidPPN), nextPageToWrite(0), clock(0) {}
  BlockMetadata(PPN id, uint32_t s)
      : blockID(id), nextPageToWrite(0), clock(0), validPages(s) {}
};

class AbstractMapping : public Object {
 protected:
  struct MemoryEntry {
    uint64_t address;
    uint32_t size;
    bool read;

    MemoryEntry(bool r, uint64_t a, uint32_t s)
        : address(a), size(s), read(r) {}
  };

  Parameter param;
  FIL::Config::NANDStructure *filparam;

  AbstractFTL *pFTL;
  BlockAllocator::AbstractAllocator *allocator;

  virtual void makeSpare(LPN, std::vector<uint8_t> &);
  virtual LPN readSpare(std::vector<uint8_t> &);

 public:
  AbstractMapping(ObjectData &);
  virtual ~AbstractMapping() {}

  virtual void initialize(AbstractFTL *, BlockAllocator::AbstractAllocator *);

  Parameter *getInfo();
  virtual LPN getPageUsage(LPN, LPN) = 0;

  // Allocator
  virtual uint32_t getValidPages(PPN) = 0;
  virtual uint16_t getAge(PPN) = 0;

  // I/O interfaces
  virtual void readMapping(Request *, Event) = 0;
  virtual void writeMapping(Request *, Event) = 0;
  virtual void invalidateMapping(Request *, Event) = 0;

  /**
   * \brief Check whether read-modify-write is needed
   *
   * Check full-sized request by getSLPN and getNLP function and determine
   * creating larger mapping table entry is possible.
   *
   * \param[in]   req   Request structure
   * \param[out]  slpn  Starting LPN of data range should be read
   * \param[out]  nlp   Number of pages should be read
   * \return  Return true if read-modify-write is required
   */
  virtual bool prepareReadModifyWrite(Request *req, LPN &slpn, uint32_t &nlp) = 0;

  // GC interfaces
  virtual void getCopyList(CopyList &, Event) = 0;
  virtual void releaseCopyList(CopyList &) = 0;
};

}  // namespace Mapping

}  // namespace SimpleSSD::FTL

#endif
