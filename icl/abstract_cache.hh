// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_ICL_ABSTRACT_CACHE_HH__
#define __SIMPLESSD_ICL_ABSTRACT_CACHE_HH__

#include "icl/icl.hh"
#include "sim/object.hh"

namespace SimpleSSD::ICL {

class AbstractCache : public Object {
 protected:
  FTL::FTL *pFTL;

  std::deque<Request> pendingQueue;

 public:
  AbstractCache(ObjectData &o, FTL::FTL *p) : Object(o), pFTL(p) {}
  virtual ~AbstractCache() {}

  virtual void enqueue(Request &&) = 0;

  void createCheckpoint(std::ostream &out) const noexcept {
    bool exist;

    uint64_t size = pendingQueue.size();
    BACKUP_SCALAR(out, size);

    for (auto &iter : pendingQueue) {
      BACKUP_SCALAR(out, iter.id);
      BACKUP_SCALAR(out, iter.sid);
      BACKUP_EVENT(out, iter.eid);
      BACKUP_SCALAR(out, iter.data);
      BACKUP_SCALAR(out, iter.address);
      BACKUP_SCALAR(out, iter.length);
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
      RESTORE_SCALAR(in, tmp.length);
      RESTORE_SCALAR(in, tmp.buffer);

      tmp.buffer = object.bufmgr->restorePointer(tmp.buffer);
    }
  }
};

}  // namespace SimpleSSD::ICL

#endif
