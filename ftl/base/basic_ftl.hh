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

    FormatContext() : eid(InvalidEventID), data(0) {}
  };

  struct ReadModifyWriteContext {
    Event originalEvent;
    uint64_t originalTag;

    bool readPending;
    bool writePending;

    uint64_t readTag;
    uint64_t writeTag;

    LPN offset;
    LPN length;

    ReadModifyWriteContext(Event e, uint64_t t)
        : originalEvent(e),
          originalTag(t),
          readPending(false),
          writePending(false),
          readTag(0),
          writeTag(0),
          offset(InvalidLPN),
          length(InvalidLPN) {}
  };

  bool mergeReadModifyWrite;
  bool allowPageRead;
  LPN mappingGranularity;

  std::list<Command *> writePendingQueue;

  bool gcInProgress;
  std::deque<PPN> gcBlockList;
  CopyList gcCopyList;
  uint64_t gcBeginAt;

  uint8_t formatInProgress;
  FormatContext fctx;

  std::list<ReadModifyWriteContext> rmwList;

  void read_find(Command &);

  Event eventReadDoFIL;
  void read_doFIL(uint64_t);

  Event eventReadFull;
  void read_readDone(uint64_t);

  void write_find(Command &);

  Event eventWriteFindDone;
  void write_findDone();

  Event eventWriteDoFIL;
  void write_doFIL(uint64_t);

  Event eventReadModifyDone;
  void write_readModifyDone();

  Event eventWriteDone;
  void write_rmwDone();

  void invalidate_find(Command &);

  Event eventInvalidateDoFIL;
  void invalidate_doFIL(uint64_t, uint64_t);

  Event eventGCTrigger;
  void gc_trigger(uint64_t);

  Event eventGCGetBlockList;
  void gc_blockinfo();

  Event eventGCRead;
  void gc_read();

  Event eventGCWriteMapping;
  void gc_write();

  Event eventGCWrite;
  void gc_writeDoFIL();

  Event eventGCWriteDone;
  void gc_writeDone();

  Event eventGCErase;
  void gc_erase();

  Event eventGCEraseDone;
  void gc_eraseDone();

  Event eventGCDone;
  void gc_done(uint64_t);

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
