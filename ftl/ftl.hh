// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_FTL_FTL_HH__
#define __SIMPLESSD_FTL_FTL_HH__

#include <deque>

#include "fil/fil.hh"
#include "ftl/allocator/abstract_allocator.hh"
#include "ftl/def.hh"
#include "ftl/mapping/abstract_mapping.hh"

namespace SimpleSSD::FTL {

/**
 * \brief FTL (Flash Translation Layer) class
 *
 * Defines abstract layer to the flash translation layer.
 */
class FTL : public Object {
 private:
  struct FormatContext {
    Event eid;
    uint64_t data;
  };

  FIL::FIL *pFIL;

  Mapping::AbstractMapping *pMapper;
  BlockAllocator::AbstractAllocator *pAllocator;

  bool gcInProgress;
  std::deque<PPN> gcList;
  BlockInfo gcBlock;
  uint32_t nextCopyIndex;
  uint8_t *gcBuffer;

  uint8_t formatInProgress;
  FormatContext fctx;

  void read_find(Request &&);

  Event eventReadMappingDone;
  void read_dofil(uint64_t);

  Event eventReadFILDone;
  void read_done(uint64_t);

  void write_find(Request &&);

  Event eventWriteMappingDone;
  void write_dofil(uint64_t);

  Event eventWriteFILDone;
  void write_done(uint64_t);

  void invalidate_find(Request &&);

  Event eventInvalidateMappingDone;
  void invalidate_dofil(uint64_t);

  Event eventGCBegin;
  void gc_trigger();

  Event eventGCListDone;
  void gc_blockinfo();

  Event eventGCRead;
  void gc_read();

  Event eventGCWriteMapping;
  void gc_write();

  Event eventGCWrite;
  void gc_writeDofil();

  Event eventGCErase;
  void gc_erase();

  Event eventGCDone;
  void gc_done();

 public:
  FTL(ObjectData &);
  FTL(const FTL &) = delete;
  FTL(FTL &&) noexcept = default;
  ~FTL();

  FTL &operator=(const FTL &) = delete;
  FTL &operator=(FTL &&) = default;

  void submit(Request &&);

  Parameter *getInfo();

  LPN getPageUsage(LPN, LPN);
  bool isGC();
  uint8_t isFormat();

  void bypass(FIL::Request &&);

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::FTL

#endif
