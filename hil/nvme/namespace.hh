// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __HIL_NVME_NAMESPACE_HH__
#define __HIL_NVME_NAMESPACE_HH__

#include <set>

#include "sim/abstract_controller.hh"
#include "sim/object.hh"

namespace SimpleSSD::HIL::NVMe {

class Subsystem;

//!< LPN Range. first = slpn, second = nlp.
using LPNRange = std::pair<uint64_t, uint64_t>;

class NamespaceInformation {
 public:
  uint64_t size;                         //!< NSZE
  uint64_t capacity;                     //!< NCAP
  uint64_t utilization;                  //!< NUSE
  uint64_t sizeInByteL;                  //<! NVMCAPL
  uint64_t sizeInByteH;                  //<! NVMCAPH
  uint8_t lbaFormatIndex;                //!< FLBAS
  uint8_t dataProtectionSettings;        //!< DPS
  uint8_t namespaceSharingCapabilities;  //!< NMIC
  uint8_t nvmSetIdentifier;              //!< NVMSETID
  uint32_t anaGroupIdentifier;           //!< ANAGRPID

  uint32_t lbaSize;
  LPNRange namespaceRange;

  NamespaceInformation();
};

union HealthInfo {
  uint8_t data[0x200];
  struct {
    uint8_t status;
    uint16_t temperature;
    uint8_t availableSpare;
    uint8_t spareThreshold;
    uint8_t lifeUsed;
    uint8_t reserved[26];
    uint64_t readL;
    uint64_t readH;
    uint64_t writeL;
    uint64_t writeH;
    uint64_t readCommandL;
    uint64_t readCommandH;
    uint64_t writeCommandL;
    uint64_t writeCommandH;
  };

  HealthInfo() { memset(data, 0, 0x200); }
};

class Namespace : public Object {
 private:
  Subsystem *subsystem;
  NamespaceInformation info;
  HealthInfo health;

  uint32_t nsid;
  std::set<ControllerID> attachList;

 public:
  Namespace(ObjectData &, Subsystem *);
  Namespace(const Namespace &) = delete;
  Namespace(Namespace &&) noexcept = default;
  ~Namespace();

  Namespace &operator=(const Namespace &) = delete;
  Namespace &operator=(Namespace &&) noexcept = default;

  uint32_t getNSID();

  void attach(ControllerID ctrlid);
  bool isAttached();
  bool isAttached(ControllerID ctrlid);

  NamespaceInformation *getInfo();
  void setInfo(uint32_t, NamespaceInformation *);

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::HIL::NVMe

#endif