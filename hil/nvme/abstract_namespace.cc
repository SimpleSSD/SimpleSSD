// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "hil/nvme/abstract_namespace.hh"

#include "util/disk.hh"

namespace SimpleSSD::HIL::NVMe {

NamespaceInformation::NamespaceInformation()
    : size(0),
      capacity(0),
      utilization(0),
      sizeInByteL(0),
      sizeInByteH(0),
      lbaFormatIndex(0),
      dataProtectionSettings(0),
      namespaceSharingCapabilities(0),
      anaGroupIdentifier(0),
      nvmSetIdentifier(0),
      readBytes(0),
      writeBytes(0),
      lbaSize(0),
      lpnSize(0),
      namespaceRange(0, 0) {}

HealthInfo::HealthInfo() {
  memset(data, 0, 0x200);
}

AbstractNamespace::AbstractNamespace(ObjectData &o)
    : Object(o),
      inited(false),
      nsid(0),
      csi(CommandSetIdentifier::Invalid),
      disk(nullptr) {}

AbstractNamespace::~AbstractNamespace() {
  delete disk;
}

uint32_t AbstractNamespace::getNSID() {
  return nsid;
}

bool AbstractNamespace::attach(ControllerID ctrlid) {
  auto ret = attachList.emplace(ctrlid);

  return ret.second;
}

bool AbstractNamespace::detach(ControllerID ctrlid) {
  auto iter = attachList.find(ctrlid);

  if (iter != attachList.end()) {
    attachList.erase(iter);

    return true;
  }

  return false;
}

bool AbstractNamespace::isAttached() {
  return attachList.size() > 0;
}

bool AbstractNamespace::isAttached(ControllerID ctrlid) {
  auto iter = attachList.find(ctrlid);

  if (iter != attachList.end()) {
    return true;
  }

  return false;
}

NamespaceInformation *AbstractNamespace::getInfo() {
  return &nsinfo;
}

void AbstractNamespace::setInfo(uint32_t _nsid, NamespaceInformation *_info,
                                Config::Disk *_disk) {
  panic_if((uint8_t)csi != _info->commandSetIdentifier,
           "Invalid command set identifier.");

  *const_cast<uint32_t *>(&nsid) = _nsid;
  nsinfo = *_info;

  if (_disk && _disk->enable) {
    if (_disk->path.length() == 0) {
      disk = new MemDisk(object);
    }
    else if (_disk->useCoW) {
      disk = new CoWDisk(object);
    }
    else {
      disk = new Disk(object);
    }

    auto diskSize = disk->open(_disk->path, nsinfo.size * nsinfo.lbaSize);

    panic_if(diskSize == 0, "Failed to open/create disk image");
    panic_if(diskSize != nsinfo.size * nsinfo.lbaSize && _disk->strict,
             "Disk size does not match with configuration");

    const char *path =
        _disk->path.length() == 0 ? "In-memory" : _disk->path.c_str();

    debugprint(Log::DebugID::HIL_NVMe,
               "NS %-5d | DISK   | %" PRIx64 "h bytes | %s", nsid, diskSize,
               path);
  }

  inited = true;
}

HealthInfo *AbstractNamespace::getHealth() {
  return &health;
}

const std::set<ControllerID> &AbstractNamespace::getAttachment() const {
  return attachList;
}

Disk *AbstractNamespace::getDisk() {
  return disk;
}

void AbstractNamespace::getStatList(std::vector<Stat> &, std::string) noexcept {
}

void AbstractNamespace::getStatValues(std::vector<double> &) noexcept {}

void AbstractNamespace::resetStatValues() noexcept {}

void AbstractNamespace::createCheckpoint(std::ostream &out) const noexcept {
  bool exist;

  BACKUP_SCALAR(out, inited);
  BACKUP_SCALAR(out, nsid);

  BACKUP_SCALAR(out, nsinfo);

  BACKUP_SCALAR(out, nsinfo.namespaceRange.first);
  BACKUP_SCALAR(out, nsinfo.namespaceRange.second);

  BACKUP_BLOB(out, health.data, 0x200);

  exist = disk != nullptr;
  BACKUP_SCALAR(out, exist);

  if (exist) {
    uint8_t type;

    if (dynamic_cast<MemDisk *>(disk)) {
      type = 3;
    }
    else if (dynamic_cast<CoWDisk *>(disk)) {
      type = 2;
    }
    else if (dynamic_cast<Disk *>(disk)) {
      type = 1;
    }
    else {
      // Actually, we cannot be here!
      abort();
    }

    BACKUP_SCALAR(out, type);

    disk->createCheckpoint(out);
  }
}

void AbstractNamespace::restoreCheckpoint(std::istream &in) noexcept {
  bool exist;

  RESTORE_SCALAR(in, inited);
  RESTORE_SCALAR(in, nsid);

  RESTORE_SCALAR(in, nsinfo);

  csi = (CommandSetIdentifier)nsinfo.commandSetIdentifier;

  uint64_t val1, val2;
  RESTORE_SCALAR(in, val1);
  RESTORE_SCALAR(in, val2);

  nsinfo.namespaceRange = LPNRange(val1, val2);

  RESTORE_BLOB(in, health.data, 0x200);

  RESTORE_SCALAR(in, exist);

  if (exist) {
    uint8_t type;

    RESTORE_SCALAR(in, type);

    switch (type) {
      case 1:
        disk = new Disk(object);
        break;
      case 2:
        disk = new CoWDisk(object);
        break;
      case 3:
        disk = new MemDisk(object);
        break;
      default:
        panic("Unexpected disk type");
    }

    disk->restoreCheckpoint(in);
  }
}

}  // namespace SimpleSSD::HIL::NVMe
