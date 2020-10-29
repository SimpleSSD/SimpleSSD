// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_HIL_NVME_ABSTRACT_NAMESPACE_HH__
#define __SIMPLESSD_HIL_NVME_ABSTRACT_NAMESPACE_HH__

#include <set>

#include "hil/nvme/def.hh"
#include "hil/nvme/queue_arbitrator.hh"
#include "sim/abstract_controller.hh"

namespace SimpleSSD {

class Disk;

namespace HIL::NVMe {

//!< LPN Range. first = slpn, second = nlp.
using LPNRange = std::pair<LPN, uint64_t>;

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
  uint8_t commandSetIdentifier;          //!< DWORD11 [31:24]
  uint32_t anaGroupIdentifier;           //!< ANAGRPID
  uint16_t nvmSetIdentifier;             //!< NVMSETID

  uint16_t kvKeySize;
  uint32_t kvValueSize;
  uint32_t kvMaxKeys;

  uint32_t znsMaxOpenZones;
  uint64_t znsZoneSize;
  uint32_t znsMaxActiveZones;

  uint32_t lbaSize;
  uint64_t readBytes;
  uint64_t writeBytes;
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

class AbstractNamespace : public Object {
 private:
  bool inited;
  const uint32_t nsid;

  std::set<ControllerID> attachList;

 protected:
  CommandSetIdentifier csi;

  NamespaceInformation nsinfo;
  HealthInfo health;

  Disk *disk;

 public:
  AbstractNamespace(ObjectData &o);
  ~AbstractNamespace();

  uint32_t getNSID();

  bool attach(ControllerID);
  bool detach(ControllerID);
  bool isAttached();
  bool isAttached(ControllerID);

  NamespaceInformation *getInfo();
  void setInfo(uint32_t, NamespaceInformation *, Config::Disk *);

  HealthInfo *getHealth();

  const std::set<ControllerID> &getAttachment() const;

  Disk *getDisk();

  virtual bool validateCommand(ControllerID, SQContext *, CQContext *) = 0;

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace HIL::NVMe

}  // namespace SimpleSSD

#endif
