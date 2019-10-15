// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_FIL_ABSTRACT_FIL_HH__
#define __SIMPLESSD_FIL_ABSTRACT_FIL_HH__

#include <cinttypes>

#include "fil/fil.hh"

namespace SimpleSSD::FIL {

class AbstractFIL : public Object {
 protected:
  FIL *parent;

  std::deque<Request> pendingQueue;

 public:
  AbstractFIL(ObjectData &o, FIL *p) : Object(o), parent(p) {}
  virtual ~AbstractFIL() {}

  virtual void enqueue(Request &) = 0;

  void createCheckpoint(std::ostream &out) const noexcept {
    uint64_t size = pendingQueue.size();
    BACKUP_SCALAR(out, size);

    for (auto &iter : pendingQueue) {
      BACKUP_SCALAR(out, iter.id);
      BACKUP_SCALAR(out, iter.sid);
      BACKUP_EVENT(out, iter.eid);
      BACKUP_SCALAR(out, iter.data);
      BACKUP_SCALAR(out, iter.address);
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

      // TODO: We need to restore buffer pointer from ICL or FTL
    }
  }
};

}  // namespace SimpleSSD::FIL

#endif
