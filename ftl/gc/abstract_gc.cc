// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "ftl/gc/abstract_gc.hh"

#include "ftl/mapping/abstract_mapping.hh"

namespace SimpleSSD::FTL::GC {

AbstractGC::AbstractGC(ObjectData &o, FTLObjectData &fo, FIL::FIL *fil)
    : Object(o), ftlobject(fo), pFIL(fil), param(nullptr), requestCounter(0) {}

AbstractGC::~AbstractGC() {}

void AbstractGC::initialize() {
  param = ftlobject.pMapping->getInfo();
}

void AbstractGC::createCheckpoint(std::ostream &out) const noexcept {
  BACKUP_SCALAR(out, requestCounter);

  uint64_t size = ongoingCopy.size();
  BACKUP_SCALAR(out, size);

  for (auto &iter : ongoingCopy) {
    BACKUP_SCALAR(out, iter.first);

    iter.second.createCheckpoint(out);
  }
}

void AbstractGC::restoreCheckpoint(std::istream &in) noexcept {
  RESTORE_SCALAR(in, requestCounter);

  uint64_t size;
  RESTORE_SCALAR(in, size);

  for (uint64_t i = 0; i < size; i++) {
    uint64_t tag;
    CopyContext ctx;

    RESTORE_SCALAR(in, tag);

    ctx.restoreCheckpoint(in);

    ongoingCopy.emplace(tag, std::move(ctx));
  }
}

}  // namespace SimpleSSD::FTL::GC
