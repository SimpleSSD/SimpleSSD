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
  std::deque<PPN> gcBlockList;
  CopyList gcCopyList;
  uint32_t nextCopyIndex;

  uint8_t formatInProgress;
  FormatContext fctx;

  void read_find(Command &);

  Event eventReadDoFIL;
  void read_doFIL(uint64_t);

  void write_find(Command &);

  Event eventWriteDoFIL;
  void write_doFIL(uint64_t);

  void invalidate_find(Command &);

  Event eventInvalidateDoFIL;
  void invalidate_doFIL(uint64_t);

  Event eventGCTrigger;
  void gc_trigger();

  Event eventGCGetBlockList;
  void gc_blockinfo();

  Event eventGCRead;
  void gc_read();

  Event eventGCWriteMapping;
  void gc_write();

  Event eventGCWrite;
  void gc_writeDoFIL();

  Event eventGCErase;
  void gc_erase();

  Event eventGCEraseDone;
  void gc_eraseDone();

  Event eventGCDone;
  void gc_done();

 public:
  BasicFTL(ObjectData &, CommandManager *, FIL::FIL *,
           Mapping::AbstractMapping *, BlockAllocator::AbstractAllocator *);
  ~BasicFTL();

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
