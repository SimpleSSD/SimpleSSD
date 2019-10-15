// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_FTL_ABSTRACT_FTL_HH__
#define __SIMPLESSD_FTL_ABSTRACT_FTL_HH__

#include <cinttypes>

#include "ftl/ftl.hh"

namespace SimpleSSD::FTL {

struct Status {
  uint64_t totalLogicalPages;
  uint64_t mappedLogicalPages;
  uint64_t freePhysicalBlocks;
};

class AbstractFTL : public Object {
 protected:
  FTL *parent;

  std::deque<Request> pendingQueue;
  Status status;

 public:
  AbstractFTL(ObjectData &o, FTL *p) : Object(o), parent(p) {}
  virtual ~AbstractFTL() {}

  virtual bool initialize() = 0;

  virtual void enqueue(Request &) = 0;
  virtual Status *getStatus(uint64_t, uint64_t) = 0;

  void createCheckpoint(std::ostream &out) const noexcept {
    uint64_t size = pendingQueue.size();
    BACKUP_SCALAR(out, size);

    for (auto &iter : pendingQueue) {
      BACKUP_SCALAR(out, iter.id);
      BACKUP_SCALAR(out, iter.sid);
      BACKUP_EVENT(out, iter.eid);
      BACKUP_SCALAR(out, iter.data);
      BACKUP_SCALAR(out, iter.address);
      BACKUP_SCALAR(out, iter.buffer);
    }
  }

  void restoreCheckpoint(std::istream &in) noexcept {
    bool exist;

    uint64_t size;
    RESTORE_SCALAR(in, size);

    for (uint64_t i = 0; i < size; i++) {
      Request tmp;

      RESTORE_SCALAR(in, tmp.id);
      RESTORE_SCALAR(in, tmp.sid);
      RESTORE_EVENT(in, tmp.eid);
      RESTORE_SCALAR(in, tmp.data);
      RESTORE_SCALAR(in, tmp.address);
      RESTORE_SCALAR(in, tmp.buffer);

      tmp.buffer = object.bufmgr->restorePointer(tmp.buffer);
    }
  }
};

}  // namespace SimpleSSD::FTL

#endif
