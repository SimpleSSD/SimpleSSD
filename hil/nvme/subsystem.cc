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
    : AbstractSubsystem(o), controllerID(0), allocatedLogicalPages(0) {
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

  // Create commands
  commandDeleteSQ = new DeleteSQ(object, this);
  commandCreateSQ = new CreateSQ(object, this);
  commandGetLogPage = new GetLogPage(object, this);
  commandDeleteCQ = new DeleteCQ(object, this);
  commandCreateCQ = new CreateCQ(object, this);
  commandIdentify = new Identify(object, this);
  commandAbort = new Abort(object, this);
  commandSetFeature = new SetFeature(object, this);
  commandGetFeature = new GetFeature(object, this);
  commandAsyncEventRequest = new AsyncEventRequest(object, this);
  commandNamespaceManagement = new NamespaceManagement(object, this);
  commandNamespaceAttachment = new NamespaceAttachment(object, this);
  commandFormatNVM = new FormatNVM(object, this);

  commandFlush = new Flush(object, this);
  commandWrite = new Write(object, this);
  commandRead = new Read(object, this);
  commandCompare = new Compare(object, this);
  commandDatasetManagement = new DatasetManagement(object, this);
}

Subsystem::~Subsystem() {
  for (auto &iter : namespaceList) {
    delete iter.second;
  }

  std::visit([](auto &&hil) { delete hil; }, pHIL);

  delete commandDeleteSQ;
  delete commandCreateSQ;
  delete commandGetLogPage;
  delete commandDeleteCQ;
  delete commandCreateCQ;
  delete commandIdentify;
  delete commandAbort;
  delete commandSetFeature;
  delete commandGetFeature;
  delete commandAsyncEventRequest;
  delete commandNamespaceManagement;
  delete commandNamespaceAttachment;
  delete commandFormatNVM;

  delete commandFlush;
  delete commandWrite;
  delete commandRead;
  delete commandCompare;
  delete commandDatasetManagement;
}

void Subsystem::scheduleAEN(AsyncEventType aet, uint8_t aei, LogPageID lid) {
  for (auto &iter : aenTo) {
    commandAsyncEventRequest->invokeAEN(iter, aet, aei, lid);
  }

  aenTo.clear();
}

bool Subsystem::_createNamespace(uint32_t nsid, Config::Disk *disk,
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
        ++iter;
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
  for (auto iter = unallocated.begin(); iter != unallocated.end(); ++iter) {
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
  info->lpnSize = logicalPageSize;
  info->sizeInByteL = requestedLogicalPages * logicalPageSize;
  info->sizeInByteH = 0;

  // Create namespace
  Namespace *pNS = new Namespace(object, this);
  pNS->setInfo(nsid, info, disk);

  auto ret = namespaceList.emplace(nsid, pNS);

  panic_if(!ret.second, "Duplicated namespace ID %u", nsid);

  debugprint(Log::DebugID::HIL_NVMe,
             "NS %-5d | CREATE | LBA size %" PRIu32 " | Capacity %" PRIu64,
             nsid, info->lbaSize, info->size);

  return true;
}

bool Subsystem::_destroyNamespace(uint32_t nsid) {
  auto iter = namespaceList.find(nsid);

  if (iter != namespaceList.end()) {
    auto info = iter->second->getInfo();
    auto list = iter->second->getAttachment();

    for (auto &ctrlid : list) {
      aenTo.push_back(ctrlid);

      auto mapping = attachmentTable.find(ctrlid);

      if (LIKELY(mapping != attachmentTable.end())) {
        auto set = mapping->second.find(nsid);

        if (LIKELY(set != mapping->second.end())) {
          mapping->second.erase(set);
        }
      }
    }

    allocatedLogicalPages -= info->namespaceRange.second;

    debugprint(Log::DebugID::HIL_NVMe, "NS %-5d | DELETE", nsid);

    delete iter->second;
    namespaceList.erase(iter);

    return true;
  }

  return false;
}

bool Subsystem::submitCommand(ControllerData *cdata, SQContext *sqc) {
  bool isAdmin = sqc->getSQID() == 0;
  uint8_t opcode = sqc->getData()->dword0.opcode;

  if (isAdmin) {
    switch ((AdminCommand)opcode) {
      case AdminCommand::DeleteIOSQ:
        commandDeleteSQ->setRequest(cdata, sqc);
        break;
      case AdminCommand::CreateIOSQ:
        commandCreateSQ->setRequest(cdata, sqc);
        break;
      case AdminCommand::GetLogPage:
        commandGetLogPage->setRequest(cdata, sqc);
        break;
      case AdminCommand::DeleteIOCQ:
        commandDeleteCQ->setRequest(cdata, sqc);
        break;
      case AdminCommand::CreateIOCQ:
        commandCreateCQ->setRequest(cdata, sqc);
        break;
      case AdminCommand::Identify:
        commandIdentify->setRequest(cdata, sqc);
        break;
      case AdminCommand::Abort:
        commandAbort->setRequest(cdata, sqc);
        break;
      case AdminCommand::SetFeatures:
        commandSetFeature->setRequest(cdata, sqc);
        break;
      case AdminCommand::GetFeatures:
        commandGetFeature->setRequest(cdata, sqc);
        break;
      case AdminCommand::AsyncEventRequest:
        commandAsyncEventRequest->setRequest(cdata, sqc);
        break;
      case AdminCommand::NamespaceManagement:
        commandNamespaceManagement->setRequest(cdata, sqc);
        break;
      case AdminCommand::NamespaceAttachment:
        commandNamespaceAttachment->setRequest(cdata, sqc);
        break;
      case AdminCommand::FormatNVM:
        commandFormatNVM->setRequest(cdata, sqc);
        break;
      default:
        return false;
    }
  }
  else {
    switch ((NVMCommand)opcode) {
      case NVMCommand::Flush:
        commandFlush->setRequest(cdata, sqc);
        break;
      case NVMCommand::Write:
        commandWrite->setRequest(cdata, sqc);
        break;
      case NVMCommand::Read:
        commandRead->setRequest(cdata, sqc);
        break;
      case NVMCommand::Compare:
        commandCompare->setRequest(cdata, sqc);
        break;
      case NVMCommand::DatasetManagement:
        commandDatasetManagement->setRequest(cdata, sqc);
        break;
      default:
        return false;
    }
  }

  return true;
}

void Subsystem::shutdownCompleted(ControllerID ctrlid) {
  // We need remove aenCommands of current controller
  commandAsyncEventRequest->clearPendingRequests(ctrlid);
}

void Subsystem::triggerDispatch(ControllerData &cdata, uint64_t limit) {
  // For performance optimization, use ControllerData instead of ControllerID
  for (uint64_t i = 0; i < limit; i++) {
    auto sqc = cdata.arbitrator->dispatch();

    // No request anymore
    if (!sqc) {
      break;
    }

    // Do command handling
    if (!submitCommand(&cdata, sqc)) {
      debugprint(Log::DebugID::HIL_NVMe_Command,
                 "CTRL %-3u | SQ %2u:%-5u | Unexpected OPCODE %u",
                 cdata.controller->getControllerID(), sqc->getSQID(),
                 sqc->getCommandID(), sqc->getData()->dword0.opcode);

      auto cqc = new CQContext();

      cqc->update(sqc);
      cqc->makeStatus(true, false, StatusType::GenericCommandStatus,
                      GenericCommandStatusCode::Invalid_Opcode);

      cdata.arbitrator->complete(cqc);
    }
  }
}

void Subsystem::complete(CommandTag command) {
  uint64_t uid = command->getGCID();

  // Complete
  auto arbitrator = command->getArbitrator();
  arbitrator->complete(command->getResponse());

  // Erase
  command->getParent()->completeRequest(command);
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
        ns.capacity = nsSize * ns.lbaSize;
      }

      // Other fields will be filled in createNamespace function
      info.size = nsSize;
      info.capacity = info.size;

      if (!_createNamespace(ns.nsid, ns.pDisk, &info)) {
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

  // First controller?
  if (controllerID == 0) {
    bool attachAll = readConfigBoolean(
        Section::HostInterface, Config::Key::NVMeAttachDefaultNamespaces);

    if (attachAll) {
      // Attach all default namespaces to current controller
      for (auto iter = namespaceList.begin(); iter != namespaceList.end();
           ++iter) {
        // Namespace -> Controller
        iter->second->attach(controllerID);

        // Controller -> Namespace
        auto mapping = attachmentTable.emplace(
            std::make_pair(controllerID, std::set<uint32_t>()));

        mapping.first->second.emplace(iter->first);
      }
    }
  }

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

HealthInfo *Subsystem::getHealth(uint32_t nsid) {
  if (nsid == NSID_NONE || nsid == NSID_ALL) {
    return &health;
  }
  else {
    auto ns = namespaceList.find(nsid);

    if (ns != namespaceList.end()) {
      return ns->second->getHealth();
    }
  }

  return nullptr;
}

uint8_t Subsystem::attachNamespace(ControllerID ctrlid, uint32_t nsid,
                                   bool dry) {
  auto ctrl = controllerList.find(ctrlid);

  if (UNLIKELY(ctrl == controllerList.end())) {
    return 1u;  // No such controller
  }

  auto ns = namespaceList.find(nsid);

  if (UNLIKELY(ns == namespaceList.end())) {
    return 2u;  // No such namespace
  }

  // Private?
  bool share = ns->second->getInfo()->namespaceSharingCapabilities == 1;

  // Namespace -> Controller
  if (ns->second->isAttached(ctrlid)) {
    return 3u;  // Namespace already attached
  }

  if (!share && ns->second->isAttached()) {
    return 4u;  // Namespace is private
  }

  if (dry) {
    return 0;
  }

  ns->second->attach(ctrlid);

  // Controller -> Namespace
  auto iter =
      attachmentTable.emplace(std::make_pair(ctrlid, std::set<uint32_t>()));

  iter.first->second.emplace(nsid);

  // For attach, send newly attached namespace ID to the log
  aenTo.push_back(ctrlid);
  ctrl->second->controller->getLogPage()->cnl.appendList(nsid);

  return 0;
}

uint8_t Subsystem::detachNamespace(ControllerID ctrlid, uint32_t nsid,
                                   bool dry) {
  auto ctrl = controllerList.find(ctrlid);

  if (UNLIKELY(ctrl == controllerList.end())) {
    return 1u;  // No such controller
  }

  auto ns = namespaceList.find(nsid);

  if (UNLIKELY(ns == namespaceList.end())) {
    return 2u;  // No such namespace
  }

  // Namespace -> Controller
  if (!ns->second->isAttached(ctrlid)) {
    return 5u;  // Namespace not attached
  }

  if (dry) {
    return 0;
  }

  ns->second->detach(ctrlid);

  // Controller -> Namespace
  auto mapping = attachmentTable.find(ctrlid);

  if (LIKELY(mapping != attachmentTable.end())) {
    auto iter = mapping->second.find(nsid);

    if (LIKELY(iter != mapping->second.end())) {
      mapping->second.erase(iter);
    }
  }

  aenTo.push_back(ctrlid);

  return 0;
}

uint8_t Subsystem::createNamespace(NamespaceInformation *info, uint32_t &nsid) {
  uint32_t max = (uint32_t)readConfigUint(Section::HostInterface,
                                          Config::Key::NVMeMaxNamespace);

  nsid = NSID_LOWEST;

  // Check format
  if (UNLIKELY(info->lbaFormatIndex >= nLBAFormat)) {
    return 1u;  // Invalid format
  }

  // Allocate a namespace ID
  for (auto &iter : namespaceList) {
    if (nsid >= iter.first) {
      ++nsid;
    }
    else {
      break;
    }
  }

  if (UNLIKELY(nsid > max)) {
    return 2u;  // No more identifier
  }

  bool ret = _createNamespace(nsid, nullptr, info);

  if (UNLIKELY(!ret)) {
    return 3u;  // Insufficient capacity
  }

  return 0;
}

uint8_t Subsystem::destroyNamespace(uint32_t nsid) {
  if (nsid == NSID_ALL) {
    allocatedLogicalPages = 0;

    for (auto &iter : namespaceList) {
      debugprint(Log::DebugID::HIL_NVMe, "NS %-5d | DELETE", iter.first);

      delete iter.second;
    }

    namespaceList.clear();

    // Collected affected controllers
    for (auto &iter : attachmentTable) {
      aenTo.emplace_back(iter.first);
    }

    attachmentTable.clear();
  }
  else {
    bool ret = _destroyNamespace(nsid);

    if (UNLIKELY(!ret)) {
      return 4u;  // Namespace not exist
    }
  }

  return 0;
}

uint8_t Subsystem::format(uint32_t nsid, FormatOption ses, uint8_t lbaf,
                          Event eid, uint64_t gcid) {
  // Update namespace structure
  auto ns = namespaceList.find(nsid);

  if (UNLIKELY(ns == namespaceList.end())) {
    return 1u;  // No such namespace
  }

  auto info = ns->second->getInfo();
  info->lbaFormatIndex = lbaf;
  info->lbaSize = lbaSize[lbaf];
  info->size = info->namespaceRange.second * logicalPageSize / info->lbaSize;
  info->capacity = info->size;
  info->utilization = 0;  // Formatted

  ns->second->format();

  // Do format
  std::visit(
      [info, ses, eid, gcid](auto &&hil) {
        hil->formatPages(info->namespaceRange.first,
                         info->namespaceRange.second, ses, eid, gcid);
      },
      pHIL);

  return 0;
}

void Subsystem::getStatList(std::vector<Stat> &, std::string) noexcept {}

void Subsystem::getStatValues(std::vector<double> &) noexcept {}

void Subsystem::resetStatValues() noexcept {}

void Subsystem::createCheckpoint(std::ostream &out) const noexcept {
  BACKUP_SCALAR(out, controllerID);
  BACKUP_SCALAR(out, logicalPageSize);
  BACKUP_SCALAR(out, totalLogicalPages);
  BACKUP_SCALAR(out, allocatedLogicalPages);

  BACKUP_BLOB(out, health.data, 0x200);

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

  // Store command status
  commandDeleteSQ->createCheckpoint(out);
  commandCreateSQ->createCheckpoint(out);
  commandGetLogPage->createCheckpoint(out);
  commandDeleteCQ->createCheckpoint(out);
  commandCreateCQ->createCheckpoint(out);
  commandIdentify->createCheckpoint(out);
  commandAbort->createCheckpoint(out);
  commandSetFeature->createCheckpoint(out);
  commandGetFeature->createCheckpoint(out);
  commandAsyncEventRequest->createCheckpoint(out);
  commandNamespaceManagement->createCheckpoint(out);
  commandNamespaceAttachment->createCheckpoint(out);
  commandFormatNVM->createCheckpoint(out);

  commandFlush->createCheckpoint(out);
  commandWrite->createCheckpoint(out);
  commandRead->createCheckpoint(out);
  commandCompare->createCheckpoint(out);
  commandDatasetManagement->createCheckpoint(out);
}

void Subsystem::restoreCheckpoint(std::istream &in) noexcept {
  RESTORE_SCALAR(in, controllerID);
  RESTORE_SCALAR(in, logicalPageSize);
  RESTORE_SCALAR(in, totalLogicalPages);
  RESTORE_SCALAR(in, allocatedLogicalPages);

  RESTORE_BLOB(in, health.data, 0x200);

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

    if (!iter.second) {
      // Remove current mapping
      iter.first->second.clear();
    }

    RESTORE_SCALAR(in, length);

    for (uint64_t j = 0; j < length; j++) {
      uint32_t nsid;

      RESTORE_SCALAR(in, nsid);
      iter.first->second.emplace(nsid);
    }
  }

  commandDeleteSQ->restoreCheckpoint(in);
  commandCreateSQ->restoreCheckpoint(in);
  commandGetLogPage->restoreCheckpoint(in);
  commandDeleteCQ->restoreCheckpoint(in);
  commandCreateCQ->restoreCheckpoint(in);
  commandIdentify->restoreCheckpoint(in);
  commandAbort->restoreCheckpoint(in);
  commandSetFeature->restoreCheckpoint(in);
  commandGetFeature->restoreCheckpoint(in);
  commandAsyncEventRequest->restoreCheckpoint(in);
  commandNamespaceManagement->restoreCheckpoint(in);
  commandNamespaceAttachment->restoreCheckpoint(in);
  commandFormatNVM->restoreCheckpoint(in);

  commandFlush->restoreCheckpoint(in);
  commandWrite->restoreCheckpoint(in);
  commandRead->restoreCheckpoint(in);
  commandCompare->restoreCheckpoint(in);
  commandDatasetManagement->restoreCheckpoint(in);
}

}  // namespace SimpleSSD::HIL::NVMe
