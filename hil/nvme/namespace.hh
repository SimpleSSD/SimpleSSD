// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_HIL_NVME_NAMESPACE_HH__
#define __SIMPLESSD_HIL_NVME_NAMESPACE_HH__

#include <set>

#include "hil/convert.hh"
#include "sim/abstract_controller.hh"
#include "sim/object.hh"

namespace SimpleSSD {

class Disk;

namespace HIL::NVMe {

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
  uint32_t anaGroupIdentifier;           //!< ANAGRPID
  uint16_t nvmSetIdentifier;             //!< NVMSETID

  uint64_t readBytes;
  uint64_t writeBytes;
  uint32_t lbaSize;
  uint64_t lpnSize;
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

  HealthInfo();
};

class Namespace : public Object {
 private:
  // Subsystem *subsystem;
  NamespaceInformation info;
  HealthInfo health;

  bool inited;

  uint32_t nsid;
  std::set<ControllerID> attachList;

 public:
  Namespace(ObjectData &, Subsystem *);
  ~Namespace();

  uint32_t getNSID();

  bool attach(ControllerID);
  bool detach(ControllerID);
  bool isAttached();
  bool isAttached(ControllerID);

  NamespaceInformation *getInfo();
  void setInfo(uint32_t, NamespaceInformation *, Config::Disk *);

  HealthInfo *getHealth();

  const std::set<ControllerID> &getAttachment() const;

  void read(uint64_t);
  void write(uint64_t);

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace HIL::NVMe

}  // namespace SimpleSSD

#endif
