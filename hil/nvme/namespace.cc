// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "hil/nvme/namespace.hh"

#include "hil/nvme/subsystem.hh"
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
      nvmSetIdentifier(0),
      anaGroupIdentifier(0),
      lbaSize(0),
      namespaceRange(0, 0) {}

HealthInfo::HealthInfo() {
  memset(data, 0, 0x200);
}

Namespace::Namespace(ObjectData &o, Subsystem *s)
    : Object(o), subsystem(s), inited(false), disk(nullptr), convert(nullptr) {}

Namespace::~Namespace() {
  delete disk;
  delete convert;
}

uint32_t Namespace::getNSID() {
  return nsid;
}

bool Namespace::attach(ControllerID ctrlid) {
  auto ret = attachList.emplace(ctrlid);

  return ret.second;
}

bool Namespace::isAttached() {
  return attachList.size() > 0;
}

bool Namespace::isAttached(ControllerID ctrlid) {
  auto iter = attachList.find(ctrlid);

  if (iter != attachList.end()) {
    return true;
  }

  return false;
}

NamespaceInformation *Namespace::getInfo() {
  return &info;
}

void Namespace::setInfo(uint32_t _nsid, NamespaceInformation *_info,
                        Config::Disk *_disk) {
  nsid = _nsid;

  info = *_info;

  if (_disk) {
    if (_disk->path.length() == 0) {
      disk = new MemDisk(object);
    }
    else if (_disk->useCoW) {
      disk = new CoWDisk(object);
    }
    else {
      disk = new Disk(object);
    }

    auto diskSize =
        disk->open(_disk->path, info.size * info.lbaSize, info.lbaSize);

    panic_if(diskSize == 0, "Failed to open/create disk image");
    panic_if(diskSize != info.size * info.lbaSize && _disk->strict,
             "Disk size does not match with configuration");
  }

  inited = true;
}

const std::set<ControllerID> &Namespace::getAttachment() const {
  return attachList;
}

void Namespace::getStatList(std::vector<Stat> &, std::string) noexcept {}

void Namespace::getStatValues(std::vector<double> &) noexcept {}

void Namespace::resetStatValues() noexcept {}

void Namespace::createCheckpoint(std::ostream &out) noexcept {
  bool exist;

  BACKUP_SCALAR(out, inited);
  BACKUP_SCALAR(out, nsid);

  BACKUP_SCALAR(out, info.size);
  BACKUP_SCALAR(out, info.capacity);
  BACKUP_SCALAR(out, info.utilization);
  BACKUP_SCALAR(out, info.sizeInByteL);
  BACKUP_SCALAR(out, info.sizeInByteH);
  BACKUP_SCALAR(out, info.lbaFormatIndex);
  BACKUP_SCALAR(out, info.dataProtectionSettings);
  BACKUP_SCALAR(out, info.namespaceSharingCapabilities);
  BACKUP_SCALAR(out, info.nvmSetIdentifier);
  BACKUP_SCALAR(out, info.anaGroupIdentifier);
  BACKUP_SCALAR(out, info.lbaSize);
  BACKUP_SCALAR(out, info.namespaceRange.first);
  BACKUP_SCALAR(out, info.namespaceRange.second);

  BACKUP_BLOB(out, health.data, 0x200);

  exist = disk != nullptr;
  BACKUP_SCALAR(out, exist);

  if (exist) {
    uint8_t type;

    if constexpr (std::is_same_v<decltype(*disk), Disk>) {
      type = 1;
      BACKUP_SCALAR(out, type);
    }
    else if constexpr (std::is_same_v<decltype(*disk), CoWDisk>) {
      type = 2;
      BACKUP_SCALAR(out, type);
    }
    else if constexpr (std::is_same_v<decltype(*disk), MemDisk>) {
      type = 3;
      BACKUP_SCALAR(out, type);
    }

    disk->createCheckpoint(out);
  }

  exist = convert != nullptr;
  BACKUP_SCALAR(out, exist);

  if (convert) {
    convert->createCheckpoint(out);
  }
}

void Namespace::restoreCheckpoint(std::istream &in) noexcept {
  bool exist;

  RESTORE_SCALAR(in, inited);
  RESTORE_SCALAR(in, nsid);

  RESTORE_SCALAR(in, info.size);
  RESTORE_SCALAR(in, info.capacity);
  RESTORE_SCALAR(in, info.utilization);
  RESTORE_SCALAR(in, info.sizeInByteL);
  RESTORE_SCALAR(in, info.sizeInByteH);
  RESTORE_SCALAR(in, info.lbaFormatIndex);
  RESTORE_SCALAR(in, info.dataProtectionSettings);
  RESTORE_SCALAR(in, info.namespaceSharingCapabilities);
  RESTORE_SCALAR(in, info.nvmSetIdentifier);
  RESTORE_SCALAR(in, info.anaGroupIdentifier);
  RESTORE_SCALAR(in, info.lbaSize);

  uint64_t val1, val2;
  RESTORE_SCALAR(in, val1);
  RESTORE_SCALAR(in, val2);

  info.namespaceRange = std::move(LPNRange(val1, val2));

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

  RESTORE_SCALAR(in, exist);

  if (convert) {
    convert->restoreCheckpoint(in);
  }
}

}  // namespace SimpleSSD::HIL::NVMe
