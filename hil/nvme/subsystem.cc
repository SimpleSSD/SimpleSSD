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
#include "hil/nvme/namespace.hh"

namespace SimpleSSD::HIL::NVMe {

Subsystem::Subsystem(ObjectData &o)
    : AbstractSubsystem(o),
      controllerID(0),
      allocatedLogicalPages(0),
      dispatching(false) {
  pHIL = new HIL(o, this);

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

  // Initialize log pages
  // Fill firmware slot information
  memset(fsi.data, 0, 64);

  // Fill commands supported and effects
  memset(csae, 0, 4096);

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

  // Create event
  eventDispatch = createEvent([this](uint64_t, uint64_t) { dispatch(); },
                              "NVMe::Subsystem::eventDispatch");
}

Subsystem::~Subsystem() {
  for (auto &iter : controllerList) {
    // Other fields will be handled in dtor of controller
    delete iter.second->controller;
  }
  for (auto &iter : namespaceList) {
    delete iter.second;
  }

  delete pHIL;

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
                                 NamespaceInformation *nsinfo) {
  std::list<LPNRange> allocated;
  std::list<LPNRange> unallocated;

  // Allocate LPN
  uint64_t requestedLogicalPages =
      nsinfo->size / logicalPageSize * lbaSize[nsinfo->lbaFormatIndex];
  uint64_t unallocatedLogicalPages = totalLogicalPages - allocatedLogicalPages;

  if (requestedLogicalPages > unallocatedLogicalPages) {
    return false;
  }

  // Collect allocated slots
  for (auto &iter : namespaceList) {
    allocated.emplace_back(iter.second->getInfo()->namespaceRange);
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
  unallocated.emplace_back(LPNRange(0, totalLogicalPages));

  for (auto &iter : allocated) {
    // Split last item
    auto &last = unallocated.back();

    if (last.first <= iter.first &&
        last.first + last.second >= iter.first + iter.second) {
      unallocated.emplace_back(
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
      nsinfo->namespaceRange = *iter;
      nsinfo->namespaceRange.second = requestedLogicalPages;

      break;
    }
  }

  if (nsinfo->namespaceRange.second == 0) {
    return false;
  }

  allocatedLogicalPages += requestedLogicalPages;

  // Fill Information
  nsinfo->lpnSize = logicalPageSize;
  nsinfo->sizeInByteL = requestedLogicalPages * logicalPageSize;
  nsinfo->sizeInByteH = 0;

  // Create namespace
  Namespace *pNS = nullptr;

  switch ((CommandSetIdentifier)nsinfo->commandSetIdentifier) {
    case CommandSetIdentifier::NVM:
      pNS = new Namespace(object);

      break;
    default:
      panic("Invalid command set identifier.");

      break;
  }

  pNS->setInfo(nsid, nsinfo, disk);

  auto ret = namespaceList.emplace(nsid, pNS);

  panic_if(!ret.second, "Duplicated namespace ID %u", nsid);

  debugprint(Log::DebugID::HIL_NVMe,
             "NS %-5d | CREATE | LBA size %" PRIu32 " | Capacity %" PRIu64,
             nsid, nsinfo->lbaSize, nsinfo->size);

  return true;
}

bool Subsystem::_destroyNamespace(uint32_t nsid) {
  auto iter = namespaceList.find(nsid);

  if (iter != namespaceList.end()) {
    auto nsinfo = iter->second->getInfo();
    auto list = iter->second->getAttachment();

    for (auto &ctrlid : list) {
      aenTo.emplace_back(ctrlid);

      auto mapping = attachmentTable.find(ctrlid);

      if (LIKELY(mapping != attachmentTable.end())) {
        auto set = mapping->second.find(nsid);

        if (LIKELY(set != mapping->second.end())) {
          mapping->second.erase(set);
        }
      }
    }

    allocatedLogicalPages -= nsinfo->namespaceRange.second;

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
  uint32_t nsid = sqc->getData()->namespaceID;
  AbstractNamespace *ns = nullptr;
  CommandSetIdentifier csi = CommandSetIdentifier::NVM;

  // Get Namespace
  if (nsid != NSID_ALL && nsid != NSID_NONE) {
    auto iter = namespaceList.find(nsid);

    if (LIKELY(iter != namespaceList.end())) {
      ns = iter->second;
      csi = (CommandSetIdentifier)ns->getInfo()->commandSetIdentifier;
    }
  }

  if (isAdmin) {
    if (csi == CommandSetIdentifier::NVM) {
      // NVM Command Set only commands
      switch ((AdminCommand)opcode) {
        case AdminCommand::FormatNVM:
          commandFormatNVM->setRequest(cdata, ns, sqc);

          return true;
      }
    }

    switch ((AdminCommand)opcode) {
      case AdminCommand::DeleteIOSQ:
        commandDeleteSQ->setRequest(cdata, ns, sqc);
        break;
      case AdminCommand::CreateIOSQ:
        commandCreateSQ->setRequest(cdata, ns, sqc);
        break;
      case AdminCommand::GetLogPage:
        commandGetLogPage->setRequest(cdata, ns, sqc);
        break;
      case AdminCommand::DeleteIOCQ:
        commandDeleteCQ->setRequest(cdata, ns, sqc);
        break;
      case AdminCommand::CreateIOCQ:
        commandCreateCQ->setRequest(cdata, ns, sqc);
        break;
      case AdminCommand::Identify:
        commandIdentify->setRequest(cdata, ns, sqc);
        break;
      case AdminCommand::Abort:
        commandAbort->setRequest(cdata, ns, sqc);
        break;
      case AdminCommand::SetFeatures:
        commandSetFeature->setRequest(cdata, ns, sqc);
        break;
      case AdminCommand::GetFeatures:
        commandGetFeature->setRequest(cdata, ns, sqc);
        break;
      case AdminCommand::AsyncEventRequest:
        commandAsyncEventRequest->setRequest(cdata, ns, sqc);
        break;
      case AdminCommand::NamespaceManagement:
        commandNamespaceManagement->setRequest(cdata, ns, sqc);
        break;
      case AdminCommand::NamespaceAttachment:
        commandNamespaceAttachment->setRequest(cdata, ns, sqc);
        break;
      default:
        return false;
    }
  }
  else {
    if (csi == CommandSetIdentifier::KeyValue) {
      switch ((IOCommand)opcode) {
        case IOCommand::Store:
        case IOCommand::Retrieve:
        case IOCommand::Delete:
        case IOCommand::Exist:
        case IOCommand::List:
          panic("Key Value commands not implemented.");

          return true;
        case IOCommand::Compare:
        case IOCommand::DatasetManagement:
          // Not supported in Key Value Command Set
          return false;
      }
    }
    else if (csi == CommandSetIdentifier::ZonedNamespace) {
      switch ((IOCommand)opcode) {
        case IOCommand::ZoneManagementSend:
        case IOCommand::ZoneManagementReceive:
        case IOCommand::ZoneAppend:
          panic("Zoned namespace commands not implemented.");

          return true;
      }
    }

    switch ((IOCommand)opcode) {
      case IOCommand::Flush:
        commandFlush->setRequest(cdata, ns, sqc);
        break;
      case IOCommand::Write:
        commandWrite->setRequest(cdata, ns, sqc);
        break;
      case IOCommand::Read:
        commandRead->setRequest(cdata, ns, sqc);
        break;
      case IOCommand::Compare:
        commandCompare->setRequest(cdata, ns, sqc);
        break;
      case IOCommand::DatasetManagement:
        commandDatasetManagement->setRequest(cdata, ns, sqc);
        break;
      default:
        return false;
    }
  }

  return true;
}

void Subsystem::dispatch() {
  CPU::Function fstat;
  CPU::markFunction(fstat);

  Event nextEvent = eventDispatch;
  bool one = false;

  // Check all controller's queue
  for (auto &iter : controllerList) {
    auto sqc = iter.second->arbitrator->dispatch();

    if (LIKELY(sqc)) {
      one = true;

      if (UNLIKELY(!submitCommand(iter.second, sqc))) {
        debugprint(Log::DebugID::HIL_NVMe_Command,
                   "CTRL %-3u | SQ %2u:%-5u | Unexpected OPCODE %u",
                   iter.second->controller->getControllerID(), sqc->getSQID(),
                   sqc->getCommandID(), sqc->getData()->dword0.opcode);

        auto cqc = new CQContext();

        cqc->update(sqc);
        cqc->makeStatus(true, false, StatusType::GenericCommandStatus,
                        GenericCommandStatusCode::Invalid_Opcode);

        iter.second->arbitrator->complete(cqc);
      }
    }
  }

  if (UNLIKELY(!one)) {
    dispatching = false;
    nextEvent = InvalidEventID;
  }

  scheduleFunction(CPU::CPUGroup::HostInterface, nextEvent, fstat);
}

void Subsystem::shutdownCompleted(ControllerID ctrlid) {
  // We need remove aenCommands of current controller
  commandAsyncEventRequest->clearPendingRequests(ctrlid);
}

void Subsystem::triggerDispatch() {
  if (!dispatching) {
    scheduleNow(eventDispatch);
  }

  dispatching = true;
}

void Subsystem::complete(CommandTag command) {
  // Complete
  auto arbitrator = command->getArbitrator();
  arbitrator->complete(command->getResponse());

  // Erase
  command->getParent()->completeRequest(command);
}

void Subsystem::init() {
  uint32_t nNamespaces = (uint32_t)readConfigUint(
      Section::HostInterface, Config::Key::NVMeDefaultNamespace);
  uint64_t totalByteSize;

  totalLogicalPages = pHIL->getTotalPages();
  logicalPageSize = pHIL->getLPNSize();

  totalByteSize = totalLogicalPages * logicalPageSize;

  if (nNamespaces > 0) {
    NamespaceInformation nsinfo;
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
      for (nsinfo.lbaFormatIndex = 0; nsinfo.lbaFormatIndex < nLBAFormat;
           nsinfo.lbaFormatIndex++) {
        if (lbaSize[nsinfo.lbaFormatIndex] == ns.lbaSize) {
          nsinfo.lbaSize = lbaSize[nsinfo.lbaFormatIndex];

          break;
        }
      }

      panic_if(nsinfo.lbaFormatIndex == nLBAFormat,
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
      nsinfo.size = nsSize;
      nsinfo.capacity = nsinfo.size;

      if (!_createNamespace(ns.nsid, ns.pDisk, &nsinfo)) {
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
        auto mapping =
            attachmentTable.emplace(controllerID, std::set<uint32_t>());

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

void Subsystem::getGCHint(FTL::GC::HintContext &hint) noexcept {
  for (auto &iter : controllerList) {
    iter.second->arbitrator->getHint(hint);
  }
}

HIL *Subsystem::getHIL() const {
  return pHIL;
}

const std::map<uint32_t, AbstractNamespace *> &Subsystem::getNamespaceList()
    const {
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

void Subsystem::getFirmwareInfo(uint8_t *buffer, uint32_t offset,
                                uint32_t length) {
  uint8_t *src = (uint8_t *)fsi.data;

  if (offset < 64) {
    length = MIN(length, 64 - offset);

    memcpy(buffer, src + offset, length);
  }
  else {
    memset(buffer, 0, length);
  }
}

void Subsystem::getCommandEffects(uint8_t *buffer, uint32_t offset,
                                  uint32_t length) {
  uint8_t *src = (uint8_t *)csae;

  if (offset < 2048) {
    length = MIN(length, 2048 - offset);

    memcpy(buffer, src + offset, length);
  }
  else {
    memset(buffer, 0, length);
  }
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
  auto iter = attachmentTable.emplace(ctrlid, std::set<uint32_t>());

  iter.first->second.emplace(nsid);

  // For attach, send newly attached namespace ID to the log
  aenTo.emplace_back(ctrlid);
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

  aenTo.emplace_back(ctrlid);

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

void Subsystem::getStatList(std::vector<Stat> &list,
                            std::string prefix) noexcept {
  auto nprefix = prefix + "hil.nvme.";

  // All controllers
  for (auto &ctrl : controllerList) {
    std::string ctrlname("ctrl");

    ctrlname += std::to_string(ctrl.first);
    ctrlname += ".";

    ctrl.second->controller->getStatList(list, nprefix + ctrlname);
    ctrl.second->arbitrator->getStatList(list, nprefix + ctrlname + "arb.");
    ctrl.second->interruptManager->getStatList(list,
                                               nprefix + ctrlname + "intr.");
    ctrl.second->dmaEngine->getStatList(list, nprefix + ctrlname + "dma.");
  }

  // All commands
  commandDeleteSQ->getStatList(list, nprefix + "admin.deletesq.");
  commandCreateSQ->getStatList(list, nprefix + "admin.createsq.");
  commandGetLogPage->getStatList(list, nprefix + "admin.getlogpage.");
  commandDeleteCQ->getStatList(list, nprefix + "admin.deletecq.");
  commandCreateCQ->getStatList(list, nprefix + "admin.createcq.");
  commandIdentify->getStatList(list, nprefix + "admin.identify.");
  commandAbort->getStatList(list, nprefix + "admin.abort.");
  commandSetFeature->getStatList(list, nprefix + "admin.setfeature.");
  commandGetFeature->getStatList(list, nprefix + "admin.getfeature.");
  commandAsyncEventRequest->getStatList(list, nprefix + "admin.asynceventreq.");
  commandNamespaceManagement->getStatList(list, nprefix + "admin.nsmgmt.");
  commandNamespaceAttachment->getStatList(list, nprefix + "admin.nsattach.");
  commandFormatNVM->getStatList(list, nprefix + "admin.format.");
  commandFlush->getStatList(list, nprefix + "nvm.flush.");
  commandWrite->getStatList(list, nprefix + "nvm.write.");
  commandRead->getStatList(list, nprefix + "nvm.read.");
  commandCompare->getStatList(list, nprefix + "nvm.compare.");
  commandDatasetManagement->getStatList(list, nprefix + "nvm.datasetmgmt.");

  // HIL
  pHIL->getStatList(list, prefix);
}

void Subsystem::getStatValues(std::vector<double> &values) noexcept {
  // All controllers
  for (auto &ctrl : controllerList) {
    ctrl.second->controller->getStatValues(values);
    ctrl.second->arbitrator->getStatValues(values);
    ctrl.second->interruptManager->getStatValues(values);
    ctrl.second->dmaEngine->getStatValues(values);
  }

  // All commands
  commandDeleteSQ->getStatValues(values);
  commandCreateSQ->getStatValues(values);
  commandGetLogPage->getStatValues(values);
  commandDeleteCQ->getStatValues(values);
  commandCreateCQ->getStatValues(values);
  commandIdentify->getStatValues(values);
  commandAbort->getStatValues(values);
  commandSetFeature->getStatValues(values);
  commandGetFeature->getStatValues(values);
  commandAsyncEventRequest->getStatValues(values);
  commandNamespaceManagement->getStatValues(values);
  commandNamespaceAttachment->getStatValues(values);
  commandFormatNVM->getStatValues(values);
  commandFlush->getStatValues(values);
  commandWrite->getStatValues(values);
  commandRead->getStatValues(values);
  commandCompare->getStatValues(values);
  commandDatasetManagement->getStatValues(values);

  // HIL
  pHIL->getStatValues(values);
}

void Subsystem::resetStatValues() noexcept {
  // All controllers
  for (auto &ctrl : controllerList) {
    ctrl.second->controller->resetStatValues();
    ctrl.second->arbitrator->resetStatValues();
    ctrl.second->interruptManager->resetStatValues();
    ctrl.second->dmaEngine->resetStatValues();
  }

  // All commands
  commandDeleteSQ->resetStatValues();
  commandCreateSQ->resetStatValues();
  commandGetLogPage->resetStatValues();
  commandDeleteCQ->resetStatValues();
  commandCreateCQ->resetStatValues();
  commandIdentify->resetStatValues();
  commandAbort->resetStatValues();
  commandSetFeature->resetStatValues();
  commandGetFeature->resetStatValues();
  commandAsyncEventRequest->resetStatValues();
  commandNamespaceManagement->resetStatValues();
  commandNamespaceAttachment->resetStatValues();
  commandFormatNVM->resetStatValues();
  commandFlush->resetStatValues();
  commandWrite->resetStatValues();
  commandRead->resetStatValues();
  commandCompare->resetStatValues();
  commandDatasetManagement->resetStatValues();

  // HIL
  pHIL->resetStatValues();
}

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
    BACKUP_SCALAR(out, iter.second->getInfo()->commandSetIdentifier);

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

  BACKUP_EVENT(out, eventDispatch);

  // Store HIL
  pHIL->createCheckpoint(out);
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

  CommandSetIdentifier csi = CommandSetIdentifier::Invalid;

  for (uint64_t i = 0; i < size; i++) {
    AbstractNamespace *ns = nullptr;

    RESTORE_SCALAR(in, csi);

    switch (csi) {
      case CommandSetIdentifier::NVM:
        ns = new Namespace(object);
        break;
      default:
        panic("Invalid command set identifier.");
        break;
    }

    ns->restoreCheckpoint(in);

    auto iter = namespaceList.find(ns->getNSID());

    if (iter != namespaceList.end()) {
      delete iter->second;
      iter->second = ns;
    }
    else {
      namespaceList.emplace(ns->getNSID(), ns);
    }
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

  RESTORE_EVENT(in, eventDispatch);

  pHIL->restoreCheckpoint(in);

  // Clear restore buffer
  for (auto &iter : controllerList) {
    iter.second->dmaEngine->clearOldDMATagList();
  }
}

#define RESTORE_REQUEST(cmd, tag, ret)                                         \
  {                                                                            \
    (ret) = (cmd)->restoreRequest(tag);                                        \
                                                                               \
    if (ret) {                                                                 \
      return (ret);                                                            \
    }                                                                          \
  }

Request *Subsystem::restoreRequest(uint64_t tag) noexcept {
  Request *ret = nullptr;

  RESTORE_REQUEST(commandDeleteSQ, tag, ret);
  RESTORE_REQUEST(commandCreateSQ, tag, ret);
  RESTORE_REQUEST(commandGetLogPage, tag, ret);
  RESTORE_REQUEST(commandDeleteCQ, tag, ret);
  RESTORE_REQUEST(commandCreateCQ, tag, ret);
  RESTORE_REQUEST(commandIdentify, tag, ret);
  RESTORE_REQUEST(commandAbort, tag, ret);
  RESTORE_REQUEST(commandSetFeature, tag, ret);
  RESTORE_REQUEST(commandGetFeature, tag, ret);
  RESTORE_REQUEST(commandAsyncEventRequest, tag, ret);
  RESTORE_REQUEST(commandNamespaceManagement, tag, ret);
  RESTORE_REQUEST(commandNamespaceAttachment, tag, ret);
  RESTORE_REQUEST(commandFormatNVM, tag, ret);

  RESTORE_REQUEST(commandFlush, tag, ret);
  RESTORE_REQUEST(commandWrite, tag, ret);
  RESTORE_REQUEST(commandRead, tag, ret);
  RESTORE_REQUEST(commandCompare, tag, ret);
  RESTORE_REQUEST(commandDatasetManagement, tag, ret);

  return ret;
}

}  // namespace SimpleSSD::HIL::NVMe
