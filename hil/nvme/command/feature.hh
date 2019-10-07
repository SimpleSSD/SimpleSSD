// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_HIL_NVME_COMMAND_FEATURE_HH__
#define __SIMPLESSD_HIL_NVME_COMMAND_FEATURE_HH__

#include <array>

#include "hil/nvme/command/abstract_command.hh"

namespace SimpleSSD::HIL::NVMe {

class Feature : public Object {
 public:
  // See NVMe 1.4 Section 7.1 Figure 422/423
  // Feature ID  Mandatory  Description -> Supported by SimpleSSD?
  // 01h M Arbitration -> data.arbitrator
  // 02h M Power Management -> Just store value
  union {
    uint32_t data;
    struct {
      uint32_t ps : 5;  // Power State
      uint32_t wh : 3;  // Workload Hint
      uint32_t rsvd : 24;
    };
  } pm;

  // 03h O LBA Range Type
  // 04h M Temperature Threshold -> Just store value
  std::array<uint16_t, 9> overThresholdList;
  std::array<uint16_t, 9> underThresholdList;

  // 05h M Error Recovery -> Just store value
  union {
    uint32_t data;
    struct {
      uint32_t tler : 16;  // Time Limited Error Recovery
      uint32_t
          dulbe : 1;  // Deallocated or Unwritten Logical Block Error Enable
      uint32_t rsvd : 15;
    };
  } er;

  // 06h O Volatile Write Cache -> data.subsystem
  // 07h M Number of Queues -> data.arbitrator
  union NumberOfQueues {
    uint32_t data;
    struct {
      uint16_t nsq;  // Number of I/O Submission Queues
      uint16_t ncq;  // Number of I/O Completion Queues
    };
  };

  // 08h M Interrupt Coalescing -> data.interrupt
  union InterruptCoalescing {
    uint32_t data;
    struct {
      uint32_t thr : 8;   // Aggregation Threshold
      uint32_t time : 8;  // Aggregation Time
      uint32_t rsvd : 16;
    };
  };

  // 09h M Interrupt Vector Configuration -> data.interrupt
  union InterruptVectorConfiguration {
    uint32_t data;
    struct {
      uint32_t iv : 16;  // Interrupt Vector
      uint32_t cd : 1;   // Coalescing Disable
      uint32_t rsvd : 15;
    };
  };

  // 0Ah M Write Atomicity Normal -> Just store value
  uint32_t wan;

  // 0Bh M Asynchronous Event Configuration -> data.subsystem
  union {
    uint32_t data;
    struct {
      uint32_t smart : 8;    // SMART/Health Critical Warnings
      uint32_t nan : 1;      // Namespace Attribute Notices
      uint32_t fw : 1;       // Firmware Activation Notices
      uint32_t tln : 1;      // Telemetry Log Notices
      uint32_t anacn : 1;    // Asymmetric Namespace Access Change Notices
      uint32_t plealcn : 1;  // Predictable Latency Event Aggregate Log Change
                             // Notices
      uint32_t lbasin : 1;   // LBA Status Information Notices
      uint32_t
          egealcn : 1;  // Endurance Group Event Aggregate Log Change Notices
      uint32_t rsvd : 17;
    };
  } aec;

  // 0Ch O Autonomous Power State Transition
  // 0Dh O Host Memory Buffer
  // 0Eh O Timestamp
  // 0Fh O Keep Alive Timer
  // 10h O Host Controlled Thermal Management
  // 11h O Non-Operational Power State Config
  // 12h O Read Recovery Level Config
  // 13h O Predictable Latency Mode Config
  // 14h O Predictable Latency Mode Window
  // 15h O LBA Status Information Report Interval
  // 16h O Host Behavior Support
  // 17h O Sanitize Config
  // 18h O Endurance Group Event Configuration
  // 80h O Software Progress Marker
  // 81h O Host Identifier
  // 82h O Reservation Notification Mask
  // 83h O Reservation Persistence
  // 84h O Namespace Write Protection Config

  Feature(ObjectData &);
  Feature(const Feature &) = delete;
  Feature(Feature &&) noexcept = default;

  Feature &operator=(const Feature &) = delete;
  Feature &operator=(Feature &&) noexcept = default;

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::HIL::NVMe

#endif
