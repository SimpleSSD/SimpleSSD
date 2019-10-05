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
      nvmSetIdentifier(0),
      anaGroupIdentifier(0),
      lbaSize(0),
      namespaceRange(0, 0) {}

HealthInfo::HealthInfo() {
  memset(data, 0, 0x200);
}

Namespace::Namespace(ObjectData &o, Subsystem *s) : Object(o), subsystem(s) {}

Namespace::~Namespace() {
  // Delete disk
}



}  // namespace SimpleSSD::HIL::NVMe
