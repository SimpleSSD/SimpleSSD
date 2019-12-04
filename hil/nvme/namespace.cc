// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "hil/nvme/namespace.hh"

#include "hil/nvme/subsystem.hh"

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
    : Object(o) /* , subsystem(s) */, inited(false) {}

Namespace::~Namespace() {}

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
  return &info;
}

void Namespace::setInfo(uint32_t _nsid, NamespaceInformation *_info) {
  nsid = _nsid;
  info = *_info;

  inited = true;
}

HealthInfo *Namespace::getHealth() {
  return &health;
}

const std::set<ControllerID> &Namespace::getAttachment() const {
  return attachList;
}

void Namespace::read(uint64_t bytesize) {
  health.readCommandL++;

  if (UNLIKELY(health.readCommandL == 0)) {
    health.readCommandH++;
  }

  info.readBytes += bytesize;
  health.readL = info.readBytes / 1000 / 512;
}

void Namespace::write(uint64_t bytesize) {
  health.writeCommandL++;

  if (UNLIKELY(health.writeCommandL == 0)) {
    health.writeCommandH++;
  }

  info.writeBytes += bytesize;
  health.writeL = info.writeBytes / 1000 / 512;
}

void Namespace::getStatList(std::vector<Stat> &, std::string) noexcept {}

void Namespace::getStatValues(std::vector<double> &) noexcept {}

void Namespace::resetStatValues() noexcept {}

void Namespace::createCheckpoint(std::ostream &out) const noexcept {
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
  BACKUP_SCALAR(out, info.anaGroupIdentifier);
  BACKUP_SCALAR(out, info.nvmSetIdentifier);
  BACKUP_SCALAR(out, info.readBytes);
  BACKUP_SCALAR(out, info.writeBytes);
  BACKUP_SCALAR(out, info.lbaSize);
  BACKUP_SCALAR(out, info.lpnSize);
  BACKUP_SCALAR(out, info.namespaceRange.first);
  BACKUP_SCALAR(out, info.namespaceRange.second);

  BACKUP_BLOB(out, health.data, 0x200);
}

void Namespace::restoreCheckpoint(std::istream &in) noexcept {
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
  RESTORE_SCALAR(in, info.anaGroupIdentifier);
  RESTORE_SCALAR(in, info.nvmSetIdentifier);
  RESTORE_SCALAR(in, info.readBytes);
  RESTORE_SCALAR(in, info.writeBytes);
  RESTORE_SCALAR(in, info.lbaSize);
  RESTORE_SCALAR(in, info.lpnSize);

  uint64_t val1, val2;
  RESTORE_SCALAR(in, val1);
  RESTORE_SCALAR(in, val2);

  info.namespaceRange = LPNRange(val1, val2);

  RESTORE_BLOB(in, health.data, 0x200);
}

}  // namespace SimpleSSD::HIL::NVMe
