// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "hil/nvme/command/log_page.hh"

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

LogPage::LogPage(ObjectData &o) : Object(o), cnl(o) {
  // Fill firmware slot information
  memset(fsi.data, 0, 64);

  // Fill commands supported and effects

  // [Bits ] Name  : Description
  // [31:20] Reserved
  // [19:19] USS   : UUID Selection Supported
  // [18:16] CSE   : Command Submission and Execution
  // [15:05] Reserved
  // [04:04] CCC   : Controller Capability Change
  // [03:03] NIC   : Namespace Inventory Change
  // [02:02] NCC   : Namespace Capability Change
  // [01:01] LBCC  : Logical Block Content Change
  // [00:00] CSUPP : Command Supported
  //             109876543210 9 876 54321098765 4 3 2 1 0

  // 00h Delete I/O Submission Queue
  csae[0x00] = 0b000000000000'0'000'00000000000'0'0'0'0'1;
  // 01h Create I/O Submission Queue
  csae[0x01] = 0b000000000000'0'000'00000000000'0'0'0'0'1;
  // 02h Get Log Page
  csae[0x02] = 0b000000000000'0'000'00000000000'0'0'0'0'1;
  // 04h Delete I/O Completion Queue
  csae[0x04] = 0b000000000000'0'000'00000000000'0'0'0'0'1;
  // 05h Create I/O Completion Queue
  csae[0x05] = 0b000000000000'0'000'00000000000'0'0'0'0'1;
  // 06h Identify
  csae[0x06] = 0b000000000000'0'000'00000000000'0'0'0'0'1;
  // 08h Abort
  csae[0x08] = 0b000000000000'0'000'00000000000'0'0'0'0'1;
  // 09h Set Features
  csae[0x09] = 0b000000000000'0'000'00000000000'0'0'0'0'1;
  // 0Ah Get Features
  csae[0x0A] = 0b000000000000'0'000'00000000000'0'0'0'0'1;
  // 0Ch Asynchronous Event Request
  csae[0x0C] = 0b000000000000'0'000'00000000000'0'0'0'0'1;
  // 0Dh Namespace Management
  csae[0x0D] = 0b000000000000'0'010'00000000000'0'0'1'0'1;
  // 15h Namespace Attachment
  csae[0x15] = 0b000000000000'0'000'00000000000'0'1'0'0'1;
  // 80h Format NVM
  csae[0x80] = 0b000000000000'0'010'00000000000'0'0'0'1'1;
  // 00h Flush
  csae[0x100] = 0b000000000000'0'001'00000000000'0'0'0'1'1;
  // 01h Write
  csae[0x101] = 0b000000000000'0'000'00000000000'0'0'0'1'1;
  // 02h Read
  csae[0x102] = 0b000000000000'0'000'00000000000'0'0'0'0'1;
  // 05h Compare
  csae[0x105] = 0b000000000000'0'000'00000000000'0'0'0'0'1;
  // 09h Dataset Management
  csae[0x109] = 0b000000000000'0'001'00000000000'0'0'0'1'1;
}

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
