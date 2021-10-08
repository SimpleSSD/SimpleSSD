// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "ftl/allocator/abstract_allocator.hh"

#include "ftl/mapping/abstract_mapping.hh"

namespace SimpleSSD::FTL::BlockAllocator {

AbstractAllocator::AbstractAllocator(ObjectData &o, FTLObjectData &fo)
    : Object(o), ftlobject(fo), param(ftlobject.pMapping->getInfo()) {}

AbstractAllocator::~AbstractAllocator() {}

void AbstractAllocator::initialize() {}

void AbstractAllocator::callEvents(const PSBN &psbn) {
  for (auto &iter : eventList) {
    scheduleNow(iter, psbn);
  }
}

void AbstractAllocator::registerBlockEraseEventListener(Event eid) {
  eventList.emplace_back(eid);
}

void AbstractAllocator::createCheckpoint(std::ostream &out) const noexcept {
  BACKUP_STL(out, eventList, iter, BACKUP_EVENT(out, iter));
}

void AbstractAllocator::restoreCheckpoint(std::istream &in) noexcept {
  RESTORE_STL_RESIZE(in, eventList, iter, RESTORE_EVENT(in, iter));
}

}  // namespace SimpleSSD::FTL::BlockAllocator
