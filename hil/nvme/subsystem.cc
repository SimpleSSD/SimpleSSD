// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "hil/nvme/subsystem.hh"

#include <list>

#include "hil/nvme/command/command.hh"
#include "hil/nvme/controller.hh"

namespace SimpleSSD::HIL::NVMe {

Subsystem::Subsystem(ObjectData &o)
    : AbstractSubsystem(o),
      controllerID(0),
      feature(o),
      allocatedLogicalPages(0) {
  // TODO: Get total physical page count from configuration
  // auto page = readConfigUint(Section::FTL, FTL::Config::Key::Page);
  // auto block = readConfigUint(Section::FTL, FTL::Config::Key::Page);
  // auto plane = readConfigUint(Section::FTL, FTL::Config::Key::Page);
  // auto die = readConfigUint(Section::FTL, FTL::Config::Key::Page);
  // auto way = readConfigUint(Section::FTL, FTL::Config::Key::Page);
  // auto channel = readConfigUint(Section::FTL, FTL::Config::Key::Page);
  // auto size = channel * way * die * plane * block * page;
  auto size = 33554432ul;  // 8 * 4 * 2 * 2 * 512 * 512

  if (size <= std::numeric_limits<uint16_t>::max()) {
    pHIL = new HIL<uint16_t>(o);
  }
  else if (size <= std::numeric_limits<uint32_t>::max()) {
    pHIL = new HIL<uint32_t>(o);
  }
  else if (size <= std::numeric_limits<uint64_t>::max()) {
    pHIL = new HIL<uint64_t>(o);
  }
  else {
    // Not possible though... (128bit?)
    panic("Too many physical page counts.");
  }
}

Subsystem::~Subsystem() {
  for (auto &iter : namespaceList) {
    delete iter.second;
  }

  std::visit([](auto &&hil) { delete hil; }, pHIL);
}

bool Subsystem::createNamespace(uint32_t nsid, Config::Disk *disk,
                                NamespaceInformation *info) {
  std::list<LPNRange> allocated;
  std::list<LPNRange> unallocated;

  // Allocate LPN
  uint64_t requestedLogicalPages =
      info->size / logicalPageSize * lbaSize[info->lbaFormatIndex];
  uint64_t unallocatedLogicalPages = totalLogicalPages - allocatedLogicalPages;

  if (requestedLogicalPages > unallocatedLogicalPages) {
    return false;
  }

  // Collect allocated slots
  for (auto &iter : namespaceList) {
    allocated.push_back(iter.second->getInfo()->namespaceRange);
  }

  // Sort
  allocated.sort([](const LPNRange &a, const LPNRange &b) -> bool {
    return a.first < b.first;
  });

  // Merge
  auto iter = allocated.begin();

  if (iter != allocated.end()) {
    auto next = iter;

    while (true) {
      next++;

      if (iter != allocated.end() || next == allocated.end()) {
        break;
      }

      if (iter->first + iter->second == next->first) {
        iter = allocated.erase(iter);
        next = iter;
      }
      else {
        iter++;
      }
    }
  }

  // Invert
  unallocated.push_back(LPNRange(0, totalLogicalPages));

  for (auto &iter : allocated) {
    // Split last item
    auto &last = unallocated.back();

    if (last.first <= iter.first &&
        last.first + last.second >= iter.first + iter.second) {
      unallocated.push_back(
          LPNRange(iter.first + iter.second,
                   last.first + last.second - iter.first - iter.second));
      last.second = iter.first - last.first;
    }
    else {
      panic("BUG");
    }
  }

  // Allocated unallocated area to namespace
  for (auto iter = unallocated.begin(); iter != unallocated.end(); iter++) {
    if (iter->second >= requestedLogicalPages) {
      info->namespaceRange = *iter;
      info->namespaceRange.second = requestedLogicalPages;

      break;
    }
  }

  if (info->namespaceRange.second == 0) {
    return false;
  }

  allocatedLogicalPages += requestedLogicalPages;

  // Fill Information
  info->sizeInByteL = requestedLogicalPages * logicalPageSize;
  info->sizeInByteH = 0;

  // Create namespace
  Namespace *pNS = new Namespace(object, this);
  pNS->setInfo(nsid, info, disk);

  auto ret = namespaceList.emplace(nsid, pNS);

  panic_if(!ret.second, "Duplicated namespace ID %u", nsid);

  // TODO: DEBUGPRINT!

  return true;
}

bool Subsystem::destroyNamespace(uint32_t nsid) {
  auto iter = namespaceList.find(nsid);

  if (iter != namespaceList.end()) {
    auto info = iter->second->getInfo();

    allocatedLogicalPages -= info->namespaceRange.second;
    // TODO: DEBUGPRINT!

    delete iter->second;
    namespaceList.erase(iter);

    return true;
  }

  return false;
}

Command *Subsystem::makeCommand(ControllerData *cdata, SQContext *sqc) {
  bool isAdmin = sqc->getSQID() == 0;
  uint8_t opcode = sqc->getData()->dword0.opcode;

  if (isAdmin) {
    switch ((AdminCommand)opcode) {
      case AdminCommand::DeleteIOSQ:
        return nullptr;
      case AdminCommand::CreateIOSQ:
        return nullptr;
      case AdminCommand::GetLogPage:
        return nullptr;
      case AdminCommand::DeleteIOCQ:
        return nullptr;
      case AdminCommand::CreateIOCQ:
        return nullptr;
      case AdminCommand::Identify:
        return new Identify(object, this, cdata);
      case AdminCommand::Abort:
        return nullptr;
      case AdminCommand::SetFeatures:
        return new SetFeature(object, this, cdata);
      case AdminCommand::GetFeatures:
        return new GetFeature(object, this, cdata);
      case AdminCommand::AsyncEventRequest:
        return nullptr;
      case AdminCommand::NamespaceManagement:
        return nullptr;
      case AdminCommand::NamespaceAttachment:
        return nullptr;
      case AdminCommand::FormatNVM:
        return nullptr;
      default:
        return nullptr;
    }
  }
  else {
    switch ((NVMCommand)opcode) {
      case NVMCommand::Flush:
        return nullptr;
      case NVMCommand::Write:
        return nullptr;
      case NVMCommand::Read:
        return nullptr;
      case NVMCommand::Compare:
        return nullptr;
      case NVMCommand::DatasetManagement:
        return nullptr;
      default:
        return nullptr;
    }
  }
}

void Subsystem::triggerDispatch(ControllerData &cdata, uint64_t limit) {
  // For performance optimization, use ControllerData instead of ControllerID

  for (uint64_t i = 0; i < limit; i++) {
    auto sqc = cdata.arbitrator->dispatch();

    // No request anymore
    if (!sqc) {
      break;
    }

    // Find command
    auto command = makeCommand(&cdata, sqc);

    // Do command handling
    if (command) {
      command->setRequest(sqc);

      ongoingCommands.push_back(command->getUniqueID(), command);
    }
    else {
      auto cqc = new CQContext();

      cqc->update(sqc);
      cqc->makeStatus(true, false, StatusType::GenericCommandStatus,
                      GenericCommandStatusCode::Invalid_Opcode);

      cdata.arbitrator->complete(cqc);
    }
  }
}

void Subsystem::complete(Command *command) {
  uint64_t uid = command->getUniqueID();

  // Complete
  auto &data = command->getCommandData();
  data.arbitrator->complete(command->getResult());

  // Erase
  delete command;
  ongoingCommands.erase(uid);
}

void Subsystem::init() {
  uint16_t nNamespaces =
      readConfigUint(Section::HostInterface, Config::Key::NVMeDefaultNamespace);
  uint64_t totalByteSize;

  std::visit(
      [this](auto &&hil) {
        totalLogicalPages = hil->getTotalPages();
        logicalPageSize = hil->getLPNSize();
      },
      pHIL);

  totalByteSize = totalLogicalPages * logicalPageSize;

  if (nNamespaces > 0) {
    NamespaceInformation info;
    uint64_t reservedSize = 0;
    uint64_t nsSize;
    uint64_t zeroCount = 0;

    auto &list = object.config->getNamespaceList();

    for (auto &ns : list) {
      reservedSize += ns.capacity;

      if (ns.capacity == 0) {
        zeroCount++;
      }
    }

    panic_if(reservedSize > totalByteSize,
             "Requested namespace size is greater than SSD size. (%" PRIu64
             " > %" PRIu64 ")",
             reservedSize, totalByteSize);

    // Evenly divide reservedSize to zero-capacity namespaces
    reservedSize = totalByteSize - reservedSize;

    for (auto &ns : list) {
      // Make FLABS
      for (info.lbaFormatIndex = 0; info.lbaFormatIndex < nLBAFormat;
           info.lbaFormatIndex++) {
        if (lbaSize[info.lbaFormatIndex] == ns.lbaSize) {
          info.lbaSize = lbaSize[info.lbaFormatIndex];

          break;
        }
      }

      panic_if(info.lbaFormatIndex == nLBAFormat,
               "Failed to setting LBA size (LBA size must 512B ~ 4KB).");

      if (ns.capacity) {
        nsSize = ns.capacity / ns.lbaSize;
      }
      else {
        // Always zeroCount > 0 if we are here!
        nsSize = reservedSize / zeroCount / ns.lbaSize;
      }

      // Other fields will be filled in createNamespace function
      info.size = nsSize;
      info.capacity = info.size;

      if (createNamespace(ns.nsid, ns.pDisk, &info)) {
        // TODO: do we need to attach all namespaces to first controller?
      }
      else {
        panic("Failed to create namespace %u", ns.nsid);
      }
    }
  }

  inited = true;
}

ControllerID Subsystem::createController(Interface *interface) noexcept {
  panic_if(!inited, "Subsystem not initialized");

  auto ctrl = new Controller(object, controllerID, this, interface);

  // Insert to list
  // We use pointer because controller can update its data after creation
  controllerList.emplace(controllerID, ctrl->getControllerData());

  return controllerID++;
}

AbstractController *Subsystem::getController(ControllerID ctrlid) noexcept {
  auto iter = controllerList.find(ctrlid);

  if (iter != controllerList.end()) {
    return iter->second->controller;
  }

  return nullptr;
}

const Subsystem::HILPointer &Subsystem::getHIL() const {
  return pHIL;
}

const std::map<uint32_t, Namespace *> &Subsystem::getNamespaceList() const {
  return namespaceList;
}

const std::set<uint32_t> *Subsystem::getAttachment(ControllerID ctrlid) const {
  auto iter = attachmentTable.find(ctrlid);

  if (iter != attachmentTable.end()) {
    return &iter->second;
  }

  return nullptr;
}

const std::map<ControllerID, ControllerData *> &Subsystem::getControllerList()
    const {
  return controllerList;
}

uint32_t Subsystem::getLPNSize() const {
  return logicalPageSize;
}

uint64_t Subsystem::getTotalPages() const {
  return totalLogicalPages;
}

uint64_t Subsystem::getAllocatedPages() const {
  return allocatedLogicalPages;
}

Feature *Subsystem::getFeature() {
  return &feature;
}

void Subsystem::getStatList(std::vector<Stat> &, std::string) noexcept {}

void Subsystem::getStatValues(std::vector<double> &) noexcept {}

void Subsystem::resetStatValues() noexcept {}

void Subsystem::createCheckpoint(std::ostream &out) noexcept {
  BACKUP_SCALAR(out, controllerID);
  BACKUP_SCALAR(out, logicalPageSize);
  BACKUP_SCALAR(out, totalLogicalPages);
  BACKUP_SCALAR(out, allocatedLogicalPages);

  feature.createCheckpoint(out);

  uint64_t size = controllerList.size();
  BACKUP_SCALAR(out, size);

  for (auto &iter : controllerList) {
    // Only store controller ID, data must be reconstructed
    BACKUP_SCALAR(out, iter.first);

    iter.second->controller->createCheckpoint(out);
  }

  size = namespaceList.size();
  BACKUP_SCALAR(out, size);

  for (auto &iter : namespaceList) {
    iter.second->createCheckpoint(out);
  }

  size = attachmentTable.size();
  BACKUP_SCALAR(out, size);

  for (auto &iter : attachmentTable) {
    BACKUP_SCALAR(out, iter.first);

    size = iter.second.size();
    BACKUP_SCALAR(out, size);

    for (auto &set : iter.second) {
      BACKUP_SCALAR(out, set);
    }
  }

  size = ongoingCommands.size();
  BACKUP_SCALAR(out, size);

  while (auto iter = (Command *)ongoingCommands.front()) {
    uint64_t uid = iter->getUniqueID();

    // Backup unique ID only -> we can reconstruct command
    BACKUP_SCALAR(out, uid);
    iter->createCheckpoint(out);

    ongoingCommands.erase(uid);

    delete iter;
  }
}

void Subsystem::restoreCheckpoint(std::istream &in) noexcept {
  RESTORE_SCALAR(in, controllerID);
  RESTORE_SCALAR(in, logicalPageSize);
  RESTORE_SCALAR(in, totalLogicalPages);
  RESTORE_SCALAR(in, allocatedLogicalPages);

  feature.restoreCheckpoint(in);

  uint64_t size;
  RESTORE_SCALAR(in, size);

  for (uint64_t i = 0; i < size; i++) {
    ControllerID id;

    RESTORE_SCALAR(in, id);

    auto iter = controllerList.find(id);

    panic_if(iter == controllerList.end(),
             "Invalid controller Id while recover controller.");

    iter->second->controller->restoreCheckpoint(in);
  }

  RESTORE_SCALAR(in, size);

  for (uint64_t i = 0; i < size; i++) {
    auto ns = new Namespace(object, this);

    ns->restoreCheckpoint(in);

    namespaceList.emplace(ns->getNSID(), ns);
  }

  RESTORE_SCALAR(in, size);

  for (uint64_t i = 0; i < size; i++) {
    ControllerID id;
    uint64_t length = 0;

    RESTORE_SCALAR(in, id);

    auto iter = attachmentTable.emplace(id, std::set<uint32_t>());

    panic_if(!iter.second, "Invalid controller ID while recover.");

    RESTORE_SCALAR(in, length);

    for (uint64_t j = 0; j < length; j++) {
      uint32_t nsid;

      RESTORE_SCALAR(in, nsid);
      iter.first->second.emplace(nsid);
    }
  }

  RESTORE_SCALAR(in, size);

  for (uint64_t i = 0; i < size; i++) {
    uint64_t uid;

    RESTORE_SCALAR(in, uid);

    // Find controller data with controller ID
    auto data = controllerList.find(uid >> 32);

    panic_if(data == controllerList.end(),
             "Unexpected controller ID while recover.");

    // Get SQContext with SQ ID | Command ID
    auto sqc = data->second->arbitrator->getRecoveredRequest((uint32_t)uid);

    panic_if(!sqc, "Invalid request ID while recover.s");

    // Re-construct command object
    auto command = makeCommand(data->second, sqc);

    command->restoreCheckpoint(in);

    // Push to queue
    ongoingCommands.push_back(uid, command);
  }
}

}  // namespace SimpleSSD::HIL::NVMe
