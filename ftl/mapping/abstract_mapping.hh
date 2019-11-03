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
#include "hil/command_manager.hh"
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
  CommandManager *commandManager;

  Parameter param;
  FIL::Config::NANDStructure *filparam;

  AbstractFTL *pFTL;
  BlockAllocator::AbstractAllocator *allocator;

  virtual void makeSpare(LPN lpn, std::vector<uint8_t> &spare);
  virtual LPN readSpare(std::vector<uint8_t> &spare);

 public:
  AbstractMapping(ObjectData &, CommandManager *);
  AbstractMapping(const AbstractMapping &) = delete;
  AbstractMapping(AbstractMapping &&) noexcept = default;
  virtual ~AbstractMapping() {}

  AbstractMapping &operator=(const AbstractMapping &) = delete;
  AbstractMapping &operator=(AbstractMapping &&) = default;

  virtual void initialize(AbstractFTL *, BlockAllocator::AbstractAllocator *);

  Parameter *getInfo();
  virtual LPN getPageUsage(LPN, LPN) = 0;

  virtual inline PPN getSPIndexFromPPN(PPN);
  virtual inline LPN getSLPNfromLPN(LPN);
  virtual inline PPN getSBFromSPPN(PPN);
  virtual inline PPN getBlockFromPPN(PPN);
  virtual inline PPN getBlockFromSB(PPN, PPN);
  virtual inline PPN getPageIndexFromSPPN(PPN);
  virtual inline PPN makeSPPN(PPN, PPN);
  virtual inline PPN makePPN(PPN, PPN, PPN);
  virtual inline LPN mappingGranularity();

  // Allocator
  virtual uint32_t getValidPages(PPN) = 0;
  virtual uint16_t getAge(PPN) = 0;

  // I/O interfaces
  virtual CPU::Function readMapping(Command &) = 0;
  virtual CPU::Function writeMapping(Command &) = 0;
  virtual CPU::Function invalidateMapping(Command &) = 0;

  inline void readMapping(Command &, Event);
  inline void writeMapping(Command &, Event);
  inline void invalidateMapping(Command &, Event);

  // GC interfaces
  virtual void getCopyList(CopyList &, Event) = 0;
  virtual void releaseCopyList(CopyList &) = 0;
};

}  // namespace Mapping

}  // namespace SimpleSSD::FTL

#endif
