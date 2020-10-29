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

Namespace::Namespace(ObjectData &o) : AbstractNamespace(o) {
  csi = CommandSetIdentifier::NVM;
}

Namespace::~Namespace() {}

bool Namespace::validateCommand(ControllerID ctrlid, SQContext *sqc,
                                CQContext *cqc) {
  bool isAdmin = sqc->getSQID() == 0;
  uint8_t opcode = sqc->getData()->dword0.opcode;
  uint32_t target = sqc->getData()->namespaceID;

  // Attachment checking
  if (!isAdmin || (isAdmin && opcode == (uint8_t)AdminCommand::FormatNVM)) {
    if (target != NSID_ALL && !isAttached(ctrlid)) {
      // Namespace not attached
      cqc->makeStatus(false, false, StatusType::CommandSpecificStatus,
                      CommandSpecificStatusCode::Invalid_Format);

      return false;
    }
  }

  // Health update
  if (isAdmin && opcode == (uint8_t)AdminCommand::FormatNVM && disk) {
    // When format, re-create disk
    delete disk;
    disk = new MemDisk(object);

    disk->open("", nsinfo.size * nsinfo.lbaSize);
  }
  else if (!isAdmin) {
    uint64_t bytesize = (sqc->getData()->dword12 & 0xFFFF) + 1;

    bytesize *= nsinfo.lbaSize;

    switch ((IOCommand)opcode) {
      case IOCommand::Read:
        health.readCommandL++;

        if (UNLIKELY(health.readCommandL == 0)) {
          health.readCommandH++;
        }

        nsinfo.readBytes += bytesize;
        health.readL = nsinfo.readBytes / 1000 / 512;
        break;
      case IOCommand::Write:
      case IOCommand::WriteZeroes:
        health.writeCommandL++;

        if (UNLIKELY(health.writeCommandL == 0)) {
          health.writeCommandH++;
        }

        nsinfo.writeBytes += bytesize;
        health.writeL = nsinfo.writeBytes / 1000 / 512;
        break;
      default:
        break;
    }
  }

  return true;
}

}  // namespace SimpleSSD::HIL::NVMe
