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

  virtual void makeSpare(LPN lpn, std::vector<uint8_t> &spare);
  virtual LPN readSpare(std::vector<uint8_t> &spare);

  virtual CPU::Function readMapping(SubRequest *) = 0;
  virtual CPU::Function writeMapping(SubRequest *) = 0;
  virtual CPU::Function invalidateMapping(SubRequest *) = 0;

 public:
  AbstractMapping(ObjectData &);
  virtual ~AbstractMapping() {}

  virtual void initialize(AbstractFTL *, BlockAllocator::AbstractAllocator *);

  Parameter *getInfo();
  virtual LPN getPageUsage(LPN, LPN) = 0;

  //! PPN -> SPIndex (Page index in superpage)
  virtual inline PPN getSPIndexFromPPN(PPN ppn) {
    return ppn % param.superpage;
  }

  //! LPN -> SLPN / PPN -> SPPN
  virtual inline LPN getSLPNfromLPN(LPN slpn) { return slpn / param.superpage; }

  //! SPPN -> SBLK
  virtual inline PPN getSBFromSPPN(PPN sppn) {
    return sppn % (param.totalPhysicalBlocks / param.superpage);
  }

  //! PPN -> BLK
  virtual inline PPN getBlockFromPPN(PPN ppn) {
    return ppn % param.totalPhysicalBlocks;
  }

  //! SBLK/SPIndex -> BLK
  virtual inline PPN getBlockFromSB(PPN sblk, PPN sp) {
    return sblk * param.superpage + sp;
  }

  //! SPPN -> Page (Page index in (super)block)
  virtual inline PPN getPageIndexFromSPPN(PPN sppn) {
    return sppn / (param.totalPhysicalBlocks / param.superpage);
  }

  //! SBLK/Page -> SPPN
  virtual inline PPN makeSPPN(PPN superblock, PPN page) {
    return superblock + page * (param.totalPhysicalBlocks / param.superpage);
  }

  //! SBLK/SPIndex/Page -> PPN
  virtual inline PPN makePPN(PPN superblock, PPN superpage, PPN page) {
    return superblock * param.superpage + superpage +
           page * param.totalPhysicalBlocks;
  }

  //! Mapping granularity
  virtual inline LPN mappingGranularity() { return param.superpage; }

  // Allocator
  virtual uint32_t getValidPages(PPN) = 0;
  virtual uint16_t getAge(PPN) = 0;

  // I/O interfaces
  inline void readMapping(SubRequest *req, Event eid) {
    CPU::Function fstat = readMapping(req);

    scheduleFunction(CPU::CPUGroup::FlashTranslationLayer, eid, req->getTag(),
                     fstat);
  }

  inline void writeMapping(SubRequest *req, Event eid) {
    CPU::Function fstat = writeMapping(req);

    scheduleFunction(CPU::CPUGroup::FlashTranslationLayer, eid, req->getTag(),
                     fstat);
  }

  inline void invalidateMapping(SubRequest *req, Event eid) {
    CPU::Function fstat = invalidateMapping(req);

    scheduleFunction(CPU::CPUGroup::FlashTranslationLayer, eid, req->getTag(),
                     fstat);
  }

  // GC interfaces
  virtual void getCopyList(CopyList &, Event) = 0;
  virtual void releaseCopyList(CopyList &) = 0;
};

}  // namespace Mapping

}  // namespace SimpleSSD::FTL

#endif
