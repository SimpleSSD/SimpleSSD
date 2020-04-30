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

Namespace::Namespace(ObjectData &o, Subsystem * /* s */)
    : Object(o) /* , subsystem(s) */, inited(false), disk(nullptr) {}

Namespace::~Namespace() {
  delete disk;
}

uint32_t Namespace::getNSID() {
  return nsid;
}

bool Namespace::attach(ControllerID ctrlid) {
  auto ret = attachList.emplace(ctrlid);

  return ret.second;
}

bool Namespace::detach(ControllerID ctrlid) {
  auto iter = attachList.find(ctrlid);

  if (iter != attachList.end()) {
    attachList.erase(iter);

    return true;
  }

  return false;
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
  return &nsinfo;
}

void Namespace::setInfo(uint32_t _nsid, NamespaceInformation *_info,
                        Config::Disk *_disk) {
  nsid = _nsid;
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

HealthInfo *Namespace::getHealth() {
  return &health;
}

const std::set<ControllerID> &Namespace::getAttachment() const {
  return attachList;
}

void Namespace::format() {
  // When format, re-create disk
  if (disk) {
    delete disk;
    disk = new MemDisk(object);

    disk->open("", nsinfo.size * nsinfo.lbaSize);
  }
}

void Namespace::read(uint64_t bytesize) {
  health.readCommandL++;

  if (UNLIKELY(health.readCommandL == 0)) {
    health.readCommandH++;
  }

  nsinfo.readBytes += bytesize;
  health.readL = nsinfo.readBytes / 1000 / 512;
}

void Namespace::write(uint64_t bytesize) {
  health.writeCommandL++;

  if (UNLIKELY(health.writeCommandL == 0)) {
    health.writeCommandH++;
  }

  nsinfo.writeBytes += bytesize;
  health.writeL = nsinfo.writeBytes / 1000 / 512;
}

Disk *Namespace::getDisk() {
  return disk;
}

void Namespace::getStatList(std::vector<Stat> &, std::string) noexcept {}

void Namespace::getStatValues(std::vector<double> &) noexcept {}

void Namespace::resetStatValues() noexcept {}

void Namespace::createCheckpoint(std::ostream &out) const noexcept {
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

void Namespace::restoreCheckpoint(std::istream &in) noexcept {
  bool exist;

  RESTORE_SCALAR(in, inited);
  RESTORE_SCALAR(in, nsid);

  RESTORE_SCALAR(in, nsinfo);

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
