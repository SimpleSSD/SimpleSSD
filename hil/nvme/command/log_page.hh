// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_HIL_NVME_COMMAND_LOG_PAGE_HH__
#define __SIMPLESSD_HIL_NVME_COMMAND_LOG_PAGE_HH__

#include <set>

#include "sim/object.hh"

namespace SimpleSSD::HIL::NVMe {

class LogPage;

class ChangedNamespaceList : public Object {
 private:
  bool overflowed;

  std::set<uint32_t> list;

 public:
  ChangedNamespaceList(ObjectData &);

  void appendList(uint32_t);
  void makeResponse(uint64_t, uint64_t, uint8_t *);

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

class LogPage : public Object {
 public:
  // See NVMe 1.4 Section 7.1 Figure 420/421
  // Log ID Mandatory Description -> Supported by SimpleSSD?
  // 01h M Error Information -> Return empty log
  // 02h M SMART/Health Information -> data.subsystem
  // 03h M Firmware Slot Information -> Return (1 slot, active)
  union FirmwareSlotInfomation {
    uint64_t data[8];
    struct {
      uint64_t afi;
      uint64_t frs[7];
    };
  } fsi;

  // 04h O Changed Namespace List -> Handles namespace attachment/management
  ChangedNamespaceList cnl;

  // 05h O Commands Supported and Effects -> Return supported commands
  uint32_t csae[1024];

  // 06h O Device Self-test
  // 07h O Telemetry Host-Initiated
  // 08h O Telemetry Controller-Initiated
  // 09h O Endurance Group Information
  // 0Ah O Predictable Latency Per NVM Set
  // 0Bh O Predictable Latency Event Aggregate
  // 0Ch O Asymmetric Namespace Access
  // 0Dh O Persistent Event
  // 0Eh O LBA Status Information
  // 0Fh O Endurance Group Event Aggregate
  // 80h O Reservation Notification
  // 81h O Sanitize Status

  LogPage(ObjectData &);

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::HIL::NVMe

#endif
