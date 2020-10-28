// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "hil/nvme/command/internal.hh"

namespace SimpleSSD::HIL::NVMe {

ChangedNamespaceList::ChangedNamespaceList(ObjectData &o)
    : Object(o), overflowed(false) {}

void ChangedNamespaceList::appendList(uint32_t nsid) {
  if (nsid == NSID_ALL || list.size() == 1024) {
    overflowed = true;

    list.clear();
  }

  if (LIKELY(!overflowed)) {
    // No check success - map stores only unique values
    list.emplace(nsid);
  }
}

void ChangedNamespaceList::makeResponse(uint64_t offset, uint64_t length,
                                        uint8_t *buffer) {
  uint64_t limit = offset + length;

  if (UNLIKELY(overflowed)) {
    if (offset == 0 && limit >= 4) {
      *(uint32_t *)buffer = NSID_ALL;
    }
  }
  else {
    limit = MIN(limit, 4096);  // Max 1024 entries
    auto iter = list.begin();

    for (uint64_t i = 0; i < limit; i += 4) {
      if (iter != list.end()) {
        if (i >= offset) {
          *(uint32_t *)(buffer + i - offset) = *iter;
        }

        ++iter;
      }
    }
  }

  list.clear();
  overflowed = false;
}

void ChangedNamespaceList::getStatList(std::vector<Stat> &,
                                       std::string) noexcept {}

void ChangedNamespaceList::getStatValues(std::vector<double> &) noexcept {}

void ChangedNamespaceList::resetStatValues() noexcept {}

void ChangedNamespaceList::createCheckpoint(std::ostream &out) const noexcept {
  BACKUP_SCALAR(out, overflowed);

  uint64_t size = list.size();
  BACKUP_SCALAR(out, size);

  for (auto &iter : list) {
    BACKUP_SCALAR(out, iter);
  }
}

void ChangedNamespaceList::restoreCheckpoint(std::istream &in) noexcept {
  RESTORE_SCALAR(in, overflowed);

  uint64_t size;
  RESTORE_SCALAR(in, size);

  for (uint64_t i = 0; i < size; i++) {
    uint32_t nsid;

    RESTORE_SCALAR(in, nsid);

    list.emplace(nsid);
  }
}

LogPage::LogPage(ObjectData &o) : Object(o), cnl(o) {}

void LogPage::getStatList(std::vector<Stat> &, std::string) noexcept {}

void LogPage::getStatValues(std::vector<double> &) noexcept {}

void LogPage::resetStatValues() noexcept {}

void LogPage::createCheckpoint(std::ostream &out) const noexcept {
  cnl.createCheckpoint(out);
}

void LogPage::restoreCheckpoint(std::istream &in) noexcept {
  cnl.restoreCheckpoint(in);
}

}  // namespace SimpleSSD::HIL::NVMe
