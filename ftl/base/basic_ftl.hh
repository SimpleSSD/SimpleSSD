// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_FTL_BASE_BASIC_FTL_HH__
#define __SIMPLESSD_FTL_BASE_BASIC_FTL_HH__

#include "ftl/base/abstract_ftl.hh"

namespace SimpleSSD::FTL {

class BasicFTL : public AbstractFTL {
 protected:
  struct FormatContext {
    Event eid;
    uint64_t data;
  };

  bool gcInProgress;
  std::deque<PPN> gcList;
  BlockInfo gcBlock;
  uint32_t nextCopyIndex;
  uint8_t *gcBuffer;

  uint8_t formatInProgress;
  FormatContext fctx;

  void read_find(uint64_t);

  Event eventReadMappingDone;
  void read_dofil(uint64_t);

  Event eventReadFILDone;
  void read_done(uint64_t);

  void write_find(uint64_t);

  Event eventWriteMappingDone;
  void write_dofil(uint64_t);

  Event eventWriteFILDone;
  void write_done(uint64_t);

  void invalidate_find(uint64_t);

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
  BasicFTL(ObjectData &, HIL::CommandManager *, FIL::FIL *);
  ~BasicFTL();

  void initialize(Mapping::AbstractMapping *,
                  BlockAllocator::AbstractAllocator *) override;

  void submit(uint64_t) override;
  bool isGC() override;
  uint8_t isFormat() override;

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::FTL

#endif
