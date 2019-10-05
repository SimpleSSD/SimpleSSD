// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "hil/nvme/subsystem.hh"

#include <list>

#include "hil/nvme/controller.hh"

namespace SimpleSSD::HIL::NVMe {

const uint32_t nLBAFormat = 4;
const uint32_t lbaFormat[nLBAFormat] = {
    0x02090000,  // 512B + 0, Good performance
    0x020A0000,  // 1KB + 0, Good performance
    0x010B0000,  // 2KB + 0, Better performance
    0x000C0000,  // 4KB + 0, Best performance
};
const uint32_t lbaSize[nLBAFormat] = {
    512,   // 512B
    1024,  // 1KB
    2048,  // 2KB
    4096,  // 4KB
};

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
  pNS->setInfo(nsid, info);

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
}

ControllerID Subsystem::createController(Interface *interface) noexcept {
  auto ctrl = new Controller(object, controllerID, this, interface);

  // Insert to list
  // We use pointer because controller can update its data after creation
  controllerList.emplace(controllerID, &ctrl->getControllerData());

  return controllerID++;
}

void Subsystem::getStatList(std::vector<Stat> &, std::string) noexcept {}

void Subsystem::getStatValues(std::vector<double> &) noexcept {}

void Subsystem::resetStatValues() noexcept {}

void Subsystem::createCheckpoint(std::ostream &out) noexcept {
  BACKUP_SCALAR(out, controllerID);
  BACKUP_SCALAR(out, logicalPageSize);
  BACKUP_SCALAR(out, totalLogicalPages);
  BACKUP_SCALAR(out, allocatedLogicalPages);

  uint64_t size = namespaceList.size();
  BACKUP_SCALAR(out, size);

  for (auto &iter : namespaceList) {
    iter.second->createCheckpoint(out);
  }
}

void Subsystem::restoreCheckpoint(std::istream &in) noexcept {
  RESTORE_SCALAR(in, controllerID);
  RESTORE_SCALAR(in, logicalPageSize);
  RESTORE_SCALAR(in, totalLogicalPages);
  RESTORE_SCALAR(in, allocatedLogicalPages);

  uint64_t size;
  RESTORE_SCALAR(in, size);

  for (uint64_t i = 0; i < size; i++) {
    auto ns = new Namespace(object, this);

    ns->restoreCheckpoint(in);

    namespaceList.emplace(ns->getNSID(), ns);
  }
}

}  // namespace SimpleSSD::HIL::NVMe
