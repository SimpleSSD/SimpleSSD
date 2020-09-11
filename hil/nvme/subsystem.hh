// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_HIL_NVME_SUBSYSTEM_HH__
#define __SIMPLESSD_HIL_NVME_SUBSYSTEM_HH__

#include <map>
#include <variant>

#include "hil/hil.hh"
#include "hil/nvme/command/command.hh"
#include "hil/nvme/def.hh"
#include "hil/nvme/namespace.hh"
#include "hil/nvme/queue_arbitrator.hh"
#include "sim/abstract_subsystem.hh"
#include "util/sorted_map.hh"

namespace SimpleSSD::HIL::NVMe {

class ControllerData;

class Subsystem : public AbstractSubsystem {
 protected:
  HIL *pHIL;

  ControllerID controllerID;

  std::map<ControllerID, ControllerData *> controllerList;
  std::map<uint32_t, Namespace *> namespaceList;
  std::map<ControllerID, std::set<uint32_t>> attachmentTable;

  // Log Pages
  HealthInfo health;
  union FirmwareSlotInfomation {
    uint64_t data[8];
    struct {
      uint64_t afi;
      uint64_t frs[7];
    };
  } fsi;
  uint32_t csae[1024];

  uint32_t logicalPageSize;
  uint64_t totalLogicalPages;
  uint64_t allocatedLogicalPages;

  // Asynchronous Event Request
  std::vector<ControllerID> aenTo;

  // See NVMe 1.4 Section 7.1 Figure 417/418
  // Opcode Mandatory Description
  // 00h M Delete I/O Submission Queue
  DeleteSQ *commandDeleteSQ;
  // 01h M Create I/O Submission Queue
  CreateSQ *commandCreateSQ;
  // 02h M Get Log Page
  GetLogPage *commandGetLogPage;
  // 04h M Delete I/O Completion Queue
  DeleteCQ *commandDeleteCQ;
  // 05h M Create I/O Completion Queue
  CreateCQ *commandCreateCQ;
  // 06h M Identify
  Identify *commandIdentify;
  // 08h M Abort
  Abort *commandAbort;
  // 09h M Set Features
  SetFeature *commandSetFeature;
  // 0Ah M Get Features
  GetFeature *commandGetFeature;
  // 0Ch M Asynchronous Event Request
  AsyncEventRequest *commandAsyncEventRequest;
  // 0Dh O Namespace Management
  NamespaceManagement *commandNamespaceManagement;
  // 10h O Firmware Commit
  // 11h O Firmware Image Download
  // 14h O Device Self-test
  // 15h O Namespace Attachment
  NamespaceAttachment *commandNamespaceAttachment;
  // 18h O Keep Alive
  // 19h O Directive Send
  // 1Ah O Directive Receive
  // 1Ch O Virtualization Management
  // 1Dh O NVMe-MI Send
  // 1Eh O NVMe-MI Receive
  // 7Ch O Doorbell Buffer Config
  // 80h O Format NVM
  FormatNVM *commandFormatNVM;
  // 81h O Security Send
  // 82h O Security Receive
  // 84h O Sanitize
  // 86h O Get LBA Status

  // See NVMe 1.4 Section 7.1 Figure 419
  // Opcode Mandatory Description
  // 00h M Flush
  Flush *commandFlush;
  // 01h M Write
  Write *commandWrite;
  // 02h M Read
  Read *commandRead;
  // 04h O Write Uncorrectable
  // 05h O Compare
  Compare *commandCompare;
  // 08h O Write Zeroes
  // 09h O Dataset Management
  DatasetManagement *commandDatasetManagement;
  // 0Ch O Verify
  // 0Dh O Reservation Register
  // 0Eh O Reservation Report
  // 11h O Reservation Acquire
  // 15h O Reservation Release

  // Command dispatcher
  bool dispatching;
  Event eventDispatch;

  void dispatch();

  bool _createNamespace(uint32_t, Config::Disk *, NamespaceInformation *);
  bool _destroyNamespace(uint32_t);

  bool submitCommand(ControllerData *, SQContext *);

 public:
  Subsystem(ObjectData &);
  virtual ~Subsystem();

  void shutdownCompleted(ControllerID);
  void complete(CommandTag);

  void triggerDispatch();

  void init() override;

  ControllerID createController(Interface *) noexcept override;
  AbstractController *getController(ControllerID) noexcept override;

  void getGCHint(FTL::GC::HintContext &) noexcept override;

  // Command interface
  HIL *getHIL() const;
  const std::map<uint32_t, Namespace *> &getNamespaceList() const;
  const std::set<uint32_t> *getAttachment(ControllerID ctrlid) const;
  const std::map<ControllerID, ControllerData *> &getControllerList() const;
  uint32_t getLPNSize() const;
  uint64_t getTotalPages() const;
  uint64_t getAllocatedPages() const;
  HealthInfo *getHealth(uint32_t);
  void getFirmwareInfo(uint8_t *, uint32_t, uint32_t);
  void getCommandEffects(uint8_t *, uint32_t, uint32_t);

  uint8_t attachNamespace(ControllerID, uint32_t, bool = true);
  uint8_t detachNamespace(ControllerID, uint32_t, bool = true);
  uint8_t createNamespace(NamespaceInformation *, uint32_t &);
  uint8_t destroyNamespace(uint32_t);

  void scheduleAEN(AsyncEventType, uint8_t, LogPageID);

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;

  Request *restoreRequest(uint64_t) noexcept override;
};

}  // namespace SimpleSSD::HIL::NVMe

#endif
