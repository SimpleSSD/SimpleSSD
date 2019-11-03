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
  struct MemoryEntry {
    Event eid;
    uint64_t tag;
    uint64_t address;
    uint64_t size;

    MemoryEntry(Event e, uint64_t t, uint64_t a, uint64_t s)
        : eid(e), tag(t), address(a), size(s) {}
  };

  CommandManager *commandManager;

  Parameter param;
  FIL::Config::NANDStructure *filparam;

  AbstractFTL *pFTL;
  BlockAllocator::AbstractAllocator *allocator;

  std::list<MemoryEntry> memoryQueue;

  virtual void makeSpare(LPN lpn, std::vector<uint8_t> &spare);
  virtual LPN readSpare(std::vector<uint8_t> &spare);

  void insertMemoryAddress(uint64_t address, uint64_t size) {
    memoryQueue.emplace_back(InvalidEventID, address, size);
  }

  void callFunction(Event eid, Event old, uint64_t tag, CPU::Function &fstat) {
    panic_if(memoryQueue.size() == 0, "FTL APIs must insert memory request.");

    auto &memreq = memoryQueue.front();

    if (memreq.size == 0) {
      memoryQueue.pop_front();

      scheduleFunction(CPU::CPUGroup::FlashTranslationLayer, old, tag, fstat);
    }
    else {
      memreq.eid = old;
      memreq.tag = tag;

      scheduleFunction(CPU::CPUGroup::FlashTranslationLayer, eid, tag, fstat);
    }
  }

  Event eventDRAMRead;
  void readDRAM();

  Event eventDRAMWrite;
  void writeDRAM();

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
  virtual CPU::Function readMapping(Command &) = 0;
  virtual CPU::Function writeMapping(Command &) = 0;
  virtual CPU::Function invalidateMapping(Command &) = 0;

  inline void readMapping(Command &cmd, Event eid) {
    CPU::Function fstat = readMapping(cmd);

    callFunction(eventDRAMRead, eid, cmd.tag, fstat);
  }

  inline void writeMapping(Command &cmd, Event eid) {
    CPU::Function fstat = writeMapping(cmd);

    callFunction(eventDRAMWrite, eid, cmd.tag, fstat);
  }

  inline void invalidateMapping(Command &cmd, Event eid) {
    CPU::Function fstat = invalidateMapping(cmd);

    callFunction(eventDRAMWrite, eid, cmd.tag, fstat);
  }

  // GC interfaces
  virtual void getCopyList(CopyList &, Event) = 0;
  virtual void releaseCopyList(CopyList &) = 0;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace Mapping

}  // namespace SimpleSSD::FTL

#endif
