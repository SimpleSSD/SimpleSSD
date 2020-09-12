// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 *         Junhyeok Jang <jhjang@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_FTL_BASE_PAGE_LEVEL_FTL_HH__
#define __SIMPLESSD_FTL_BASE_PAGE_LEVEL_FTL_HH__

#include "ftl/base/abstract_ftl.hh"

namespace SimpleSSD::FTL {

class PageLevelFTL : public AbstractFTL {
 protected:
  uint32_t pageSize;

  bool mergeReadModifyWrite;  // if True, merge RMW requests with same PPN

  uint64_t minMappingSize;

  // Pending request
  uint64_t pendingListBaseAddress;
  SuperRequest pendingList;

  std::list<SuperRequest> writeList;
  std::unordered_map<uint64_t, ReadModifyWriteContext> rmwList;
  std::deque<Request *> stalledRequestList;

  std::list<SuperRequest>::iterator getWriteContext(uint64_t);
  std::unordered_map<uint64_t, ReadModifyWriteContext>::iterator getRMWContext(
      uint64_t);

  inline LPN getAlignedLPN(LPN lpn) {
    return static_cast<LPN>(lpn / minMappingSize * minMappingSize);
  }

  // Statistics
  struct {
    uint64_t rmwCount;         // Total number of RMW operation
    uint64_t rmwMerged;        // Total number of merged RMW operation
    uint64_t rmwReadPages;     // Read pages in RMW
    uint64_t rmwWrittenPages;  // Written pages in RMW
  } stat;

  Event eventReadSubmit;
  void read_submit(uint64_t);

  Event eventReadDone;
  void read_done(uint64_t);

  Event eventWriteSubmit;
  void write_submit(uint64_t);

  Event eventWriteDone;
  void write_done(uint64_t);

  Event eventPartialReadSubmit;
  void rmw_readSubmit(uint64_t, uint64_t);

  Event eventPartialReadDone;
  void rmw_readDone(uint64_t, uint64_t);

  Event eventPartialWriteSubmit;
  void rmw_writeSubmit(uint64_t, uint64_t);

  Event eventPartialWriteDone;
  void rmw_writeDone(uint64_t, uint64_t);

  Event eventInvalidateSubmit;
  void invalidate_submit(uint64_t, uint64_t);

 public:
  PageLevelFTL(ObjectData &, FTLObjectData &, FTL *);
  virtual ~PageLevelFTL();

  void read(Request *) override;
  bool write(Request *) override;
  void invalidate(Request *) override;

  void restartStalledRequests() override;

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::FTL

#endif
