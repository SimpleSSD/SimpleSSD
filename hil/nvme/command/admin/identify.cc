// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "hil/nvme/command/internal.hh"

namespace SimpleSSD::HIL::NVMe {

Identify::Identify(ObjectData &o, Subsystem *s) : Command(o, s) {
  dmaInitEvent =
      createEvent([this](uint64_t, uint64_t gcid) { dmaInitDone(gcid); },
                  "HIL::NVMe::Identify::dmaInitEvent");
  dmaCompleteEvent =
      createEvent([this](uint64_t, uint64_t gcid) { dmaComplete(gcid); },
                  "HIL::NVMe::Identify::dmaCompleteEvent");
}

void Identify::makeEUI64(uint8_t *buffer, uint32_t nsid) {
  char eui[9] = "";

  snprintf(eui, 9, "%08x", nsid);

  memcpy(buffer, eui, 8);
}

void Identify::makeNamespaceStructure(CommandData *tag,
                                      CommandSetIdentifier csi, uint32_t nsid,
                                      bool force) {
  auto ctrlID = tag->controller->getControllerID();
  uint8_t *buffer = tag->buffer.data();

  if (nsid == NSID_ALL) {
    switch (csi) {
      case CommandSetIdentifier::NVM:
        // We support Namespace Management, so return common namespace info,
        // especially LBA format information.

        // Number of LBA Formats
        buffer[25] = nLBAFormat - 1;  // 0's based

        // LBA Formats
        memcpy(buffer + 128, lbaFormat, 4 * nLBAFormat);

        break;
      case CommandSetIdentifier::KeyValue:
      case CommandSetIdentifier::ZonedNamespace:
        warn("Current specification does not define how to create KV/Zoned "
             "namespace.");

        break;
      default:
        break;
    }
  }
  else {
    auto attachList = subsystem->getAttachment(ctrlID);
    auto iter = attachList->find(nsid);

    if (force || iter != attachList->end()) {
      auto &namespaceList = subsystem->getNamespaceList();
      auto ns = namespaceList.find(nsid);

      if (ns == namespaceList.end()) {
        // Namespace not exists
        tag->cqc->makeStatus(false, false, StatusType::CommandSpecificStatus,
                             CommandSpecificStatusCode::Invalid_Format);

        return;
      }

      auto info = ns->second->getInfo();

      if (info->commandSetIdentifier != (uint8_t)csi) {
        tag->cqc->makeStatus(false, false, StatusType::CommandSpecificStatus,
                             CommandSpecificStatusCode::Invalid_IOCommandSet);

        return;
      }

      auto pHIL = subsystem->getHIL();
      auto logicalPageSize = subsystem->getLPNSize();

      switch (csi) {
        case CommandSetIdentifier::NVM:
          // Namespace Size
          memcpy(buffer + 0, &info->size, 8);

          // Namespace Capacity
          memcpy(buffer + 8, &info->capacity, 8);

          // Namespace Utilization
          info->utilization = pHIL->getPageUsage(info->namespaceRange.first,
                                                 info->namespaceRange.second);
          info->utilization *= logicalPageSize;
          info->utilization /= info->lbaSize;

          memcpy(buffer + 16, &info->utilization, 8);

          // Namespace Features
          buffer[24] = 0x04;  // Trim supported

          // Number of LBA Formats
          buffer[25] = nLBAFormat - 1;  // 0's based

          // Formatted LBA Size
          buffer[26] = info->lbaFormatIndex;

          // End-to-end Data Protection Capabilities
          buffer[28] = info->dataProtectionSettings;

          // Namespace Multi-path I/O and Namespace Sharing Capabilities
          buffer[30] = info->namespaceSharingCapabilities;

          // NVM capacity
          memcpy(buffer + 48, &info->sizeInByteL, 8);
          memcpy(buffer + 56, &info->sizeInByteH, 8);

          // ANA Group Identifier
          *(uint32_t *)(buffer + 92) = info->anaGroupIdentifier;

          // NVM Set Identifier
          *(uint32_t *)(buffer + 100) = info->nvmSetIdentifier;

          // LBA Formats
          memcpy(buffer + 128, lbaFormat, 4 * nLBAFormat);

          // EUI64
          makeEUI64(buffer + 120, nsid);

          break;
        case CommandSetIdentifier::KeyValue:
          // Namespace Size
          memcpy(buffer + 0, &info->size, 8);

          // Namespace Utilization
          info->utilization = pHIL->getPageUsage(info->namespaceRange.first,
                                                 info->namespaceRange.second);
          info->utilization *= logicalPageSize;
          info->utilization /= info->lbaSize;

          memcpy(buffer + 16, &info->utilization, 8);

          // Namespace Features
          buffer[24] = 0x00;

          // Number of KV Formats
          buffer[25] = 0x00;

          // Namespace Multi-path I/O and Namespace Sharing Capabilities
          buffer[26] = info->namespaceSharingCapabilities;

          // ANA Group Identifier
          *(uint32_t *)(buffer + 36) = info->anaGroupIdentifier;

          // NVM Set Identifier
          *(uint32_t *)(buffer + 44) = info->nvmSetIdentifier;

          // EUI64
          makeEUI64(buffer + 64, nsid);

          // KV Formats
          *(uint16_t *)(buffer + 72 + 16 * info->lbaFormatIndex) =
              info->kvKeySize;
          *(uint32_t *)(buffer + 76 + 16 * info->lbaFormatIndex) =
              info->kvValueSize;
          *(uint32_t *)(buffer + 80 + 16 * info->lbaFormatIndex) =
              info->kvMaxKeys;

          break;
        case CommandSetIdentifier::ZonedNamespace:
          // Zone Operation Characteristics
          buffer[0] = 0x00;
          buffer[1] = 0x00;

          // Optional Zoned Command Support
          buffer[2] = 0x01;
          buffer[3] = 0x00;

          // Maximum Active Resources
          *(uint32_t *)(buffer + 4) = info->znsMaxActiveZones - 1;

          // Maximum Open Resources
          *(uint32_t *)(buffer + 8) = info->znsMaxOpenZones - 1;

          // Reset Recommanded Limit
          *(uint32_t *)(buffer + 12) = 0;

          // Finish Recommended Limit
          *(uint32_t *)(buffer + 16) = 0;

          // LBA Format Extension
          *(uint64_t *)(buffer + 2816 + 16 * info->lbaFormatIndex) =
              info->znsZoneSize;

          break;
        default:
          break;
      }
    }
    else {
      // Namespace not attached
      tag->cqc->makeStatus(true, false, StatusType::CommandSpecificStatus,
                           CommandSpecificStatusCode::NamespaceNotAttached);
    }
  }
}

void Identify::makeNamespaceDescriptor(CommandData *tag, uint32_t nsid) {
  auto ctrlID = tag->controller->getControllerID();
  uint8_t *buffer = tag->buffer.data();

  if (nsid == NSID_ALL) {
    // Invalid namespace ID
    tag->cqc->makeStatus(true, false, StatusType::GenericCommandStatus,
                         GenericCommandStatusCode::Invalid_NamespaceOrFormat);
  }
  else {
    auto attachList = subsystem->getAttachment(ctrlID);
    auto iter = attachList->find(nsid);

    if (iter != attachList->end()) {
      auto &namespaceList = subsystem->getNamespaceList();
      auto ns = namespaceList.find(nsid);

      if (ns == namespaceList.end()) {
        // Namespace not exists
        tag->cqc->makeStatus(
            false, false, StatusType::GenericCommandStatus,
            GenericCommandStatusCode::Invalid_NamespaceOrFormat);

        return;
      }

      auto info = ns->second->getInfo();

      // EUI64
      buffer[0] = 0x01;
      buffer[1] = 8;

      makeEUI64(buffer + 4, nsid);

      // CSI
      buffer[12] = 0x04;
      buffer[13] = 1;
      buffer[16] = info->commandSetIdentifier;
    }
    else {
      // Namespace not attached
      tag->cqc->makeStatus(true, false, StatusType::CommandSpecificStatus,
                           CommandSpecificStatusCode::NamespaceNotAttached);
    }
  }
}

void Identify::makeNamespaceList(CommandData *tag, CommandSetIdentifier csi,
                                 uint32_t nsid, bool force) {
  uint16_t idx = 0;
  uint8_t *buffer = tag->buffer.data();

  if (nsid >= NSID_ALL - 1) {
    // Invalid namespace ID
    tag->cqc->makeStatus(true, false, StatusType::GenericCommandStatus,
                         GenericCommandStatusCode::Invalid_Field);
  }
  else {
    auto &namespaceList = subsystem->getNamespaceList();

    if (force) {
      for (auto &iter : namespaceList) {
        if (csi != CommandSetIdentifier::NVM &&
            iter.second->getInfo()->commandSetIdentifier != (uint8_t)csi) {
          continue;
        }

        ((uint32_t *)buffer)[idx++] = iter.first;
      }
    }
    else {
      auto ctrlID = tag->controller->getControllerID();
      auto attachList = subsystem->getAttachment(ctrlID);

      // No list
      if (attachList == nullptr) {
        return;
      }

      for (auto iter = attachList->begin(); iter != attachList->end(); ++iter) {
        auto ns = namespaceList.find(*iter);

        if (csi != CommandSetIdentifier::NVM &&
            ns->second->getInfo()->commandSetIdentifier != (uint8_t)csi) {
          continue;
        }

        ((uint32_t *)buffer)[idx++] = *iter;
      }
    }
  }
}

void Identify::makeControllerStructure(CommandData *tag,
                                       CommandSetIdentifier csi) {
  uint8_t *buffer = tag->buffer.data();

  switch (csi) {
    case CommandSetIdentifier::NVM:
      makeCommonControllerStructure(tag);
      break;
    case CommandSetIdentifier::KeyValue:
      break;
    case CommandSetIdentifier::ZonedNamespace:
      // Zone Append Size Limit
      buffer[0] = 0x00;

      break;
    default:
      break;
  }
}

void Identify::makeCommonControllerStructure(CommandData *tag) {
  uint16_t vid, ssvid;
  uint16_t id;
  uint64_t totalSize;
  uint64_t unallocated;
  uint64_t lpnSize;
  uint8_t *buffer = tag->buffer.data();

  tag->interface->getPCIID(vid, ssvid);

  id = (uint16_t)tag->controller->getControllerID();

  lpnSize = subsystem->getLPNSize();
  totalSize = subsystem->getTotalPages() * lpnSize;
  unallocated = subsystem->getAllocatedPages() * lpnSize;

  unallocated = totalSize - unallocated;

  /** Controller Capabilities and Features **/
  {
    // PCI Vendor ID
    memcpy(buffer + 0x0000, &vid, 2);

    // PCI Subsystem Vendor ID
    memcpy(buffer + 0x0002, &ssvid, 2);

    // Serial Number
    memcpy(buffer + 0x0004, "00000000000000000000", 0x14);

    // Model Number
    memcpy(buffer + 0x0018, "SimpleSSD NVMe Controller by CAMELab    ", 0x28);

    // Firmware Revision
    memcpy(buffer + 0x0040, "03.01.00", 0x08);

    // Recommended Arbitration Burst
    buffer[0x0048] = 0x00;

    // IEEE OUI Identifier (Same as Inter 750)
    {
      buffer[0x0049] = 0xE4;
      buffer[0x004A] = 0xD2;
      buffer[0x004B] = 0x5C;
    }

    // Controller Multi-Path I/O and Namespace Sharing Capabilities
    // [Bits ] Description
    // [07:03] Reserved
    // [02:02] 1 for SR-IOV Virtual Function, 0 for PCI (Physical) Function
    // [01:01] 1 for more than one host may connected to NVM subsystem
    // [00:00] 1 for NVM subsystem may has more than one NVM subsystem port
    buffer[0x004C] = 0x02;

    // Maximum Data Transfer Size
    buffer[0x004D] = 0x00;  // No limit

    // Controller ID
    { *(uint16_t *)(buffer + 0x004E) = (uint16_t)id; }

    // Version
    {
      buffer[0x0050] = 0x00;
      buffer[0x0051] = 0x04;
      buffer[0x0052] = 0x01;
      buffer[0x0053] = 0x00;
    }  // NVM Express 1.4b Compliant Controller

    // RTD3 Resume Latency
    {
      buffer[0x0054] = 0x00;
      buffer[0x0055] = 0x00;
      buffer[0x0056] = 0x00;
      buffer[0x0057] = 0x00;
    }  // Not reported

    // RTD3 Enter Latency
    {
      buffer[0x0058] = 0x00;
      buffer[0x0059] = 0x00;
      buffer[0x005A] = 0x00;
      buffer[0x005B] = 0x00;
    }  // Not repotred

    // Optional Asynchronous Events Supported
    {
      // [Bits ] Description
      // [31:10] Reserved
      // [09:09] 1 for Support Firmware Activation Notice
      // [08:08] 1 for Support Namespace Attributes Notice
      // [07:00] Reserved
      buffer[0x005C] = 0x00;
      buffer[0x005D] = 0x00;
      buffer[0x005E] = 0x00;
      buffer[0x005F] = 0x00;
    }

    // Controller Attributes
    {
      // [Bits ] Description
      // [31:01] Reserved
      // [00:00] 1 for Support 128-bit Host Identifier
      buffer[0x0060] = 0x00;
      buffer[0x0061] = 0x00;
      buffer[0x0062] = 0x00;
      buffer[0x0063] = 0x00;
    }
    memset(buffer + 0x0064, 0, 156);  // Reserved
  }

  /** Admin Command Set Attributes & Optional Controller Capabilities **/
  {
    // Optional Admin Command Support
    {
      // [Bits ] Description
      // [15:04] Reserved
      // [03:03] 1 for Support Namespace Management and Namespace Attachment
      //         commands
      // [02:02] 1 for Support Firmware Commit and Firmware Image Download
      //         commands
      // [01:01] 1 for Support Format NVM command
      // [00:00] 1 for Support Security Send and Security Receive commands
      buffer[0x0100] = 0x0A;
      buffer[0x0101] = 0x00;
    }

    // Abort Command Limit
    buffer[0x0102] = 0x03;  // Recommanded value is 4 (3 + 1)

    // Asynchronous Event Request Limit
    buffer[0x0103] = 0x03;  // Recommanded value is 4 (3 + 1))

    // Firmware Updates
    // [Bits ] Description
    // [07:05] Reserved
    // [04:04] 1 for Support firmware activation without a reset
    // [03:01] The number of firmware slot
    // [00:00] 1 for First firmware slot is read only, 0 for read/write
    buffer[0x0104] = 0x00;

    // Log Page Attributes
    // [Bits ] Description
    // [07:03] Reserved
    // [02:02] 1 for Support extended buffer for Get Log Page command
    // [01:01] 1 for Support Command Effects log page
    // [00:00] 1 for Support S.M.A.R.T. / Health information log page per
    //         namespace basis
    buffer[0x0105] = 0x03;

    // Error Log Page Entries, 0's based value
    buffer[0x0106] = 63;  // 64 entries

    // Number of Power States Support, 0's based value
    buffer[0x0107] = 0x00;  // 1 states

    // Admin Vendor Specific Command Configuration
    // [Bits ] Description
    // [07:01] Reserved
    // [00:00] 1 for all vendor specific commands use the format at Figure 12.
    //         0 for format is vendor specific
    buffer[0x0108] = 0x00;

    // Autonomous Power State Transition Attributes
    // [Bits ] Description
    // [07:01] Reserved
    // [00:00] 1 for Support autonomous power state transitions
    buffer[0x0109] = 0x00;

    // Warning Composite Temperature Threshold
    {
      buffer[0x010A] = 0x00;
      buffer[0x010B] = 0x00;
    }

    // Critical Composite Temperature Threshold
    {
      buffer[0x010C] = 0x00;
      buffer[0x010D] = 0x00;
    }

    // Maximum Time for Firmware Activation
    {
      buffer[0x010E] = 0x00;
      buffer[0x010F] = 0x00;
    }

    // Host Memory Buffer Preferred Size
    {
      buffer[0x0110] = 0x00;
      buffer[0x0111] = 0x00;
      buffer[0x0112] = 0x00;
      buffer[0x0113] = 0x00;
    }

    // Host Memory Buffer Minimum Size
    {
      buffer[0x0114] = 0x00;
      buffer[0x0115] = 0x00;
      buffer[0x0116] = 0x00;
      buffer[0x0117] = 0x00;
    }

    // Total NVM Capacity
    {
      memcpy(buffer + 0x118, &totalSize, 8);
      memset(buffer + 0x120, 0, 8);
    }

    // Unallocated NVM Capacity
    {
      memcpy(buffer + 0x128, &unallocated, 8);
      memset(buffer + 0x130, 0, 8);
    }

    // Replay Protected Memory Block Support
    {
      // [Bits ] Description
      // [31:24] Access Size
      // [23:16] Total Size
      // [15:06] Reserved
      // [05:03] Authentication Method
      // [02:00] Number of RPMB Units
      buffer[0x0138] = 0x00;
      buffer[0x0139] = 0x00;
      buffer[0x013A] = 0x00;
      buffer[0x013B] = 0x00;
    }

    // Reserved
    memset(buffer + 0x013C, 0, 4);

    // Keep Alive Support
    {
      buffer[0x0140] = 0x00;
      buffer[0x0141] = 0x00;
    }

    // Reserved
    memset(buffer + 0x0142, 0, 190);
  }

  /** NVM Command Set Attributes **/
  {
    // Submission Queue Entry Size
    // [Bits ] Description
    // [07:04] Maximum Submission Queue Entry Size
    // [03:00] Minimum Submission Queue Entry Size
    buffer[0x0200] = 0x66;  // 64Bytes, 64Bytes

    // Completion Queue Entry Size
    // [Bits ] Description
    // [07:04] Maximum Completion Queue Entry Size
    // [03:00] Minimum Completion Queue Entry Size
    buffer[0x0201] = 0x44;  // 16Bytes, 16Bytes

    // Maximum Outstanding Commands
    {
      buffer[0x0202] = 0x00;
      buffer[0x0203] = 0x00;
    }

    // Number of Namespaces
    // SimpleSSD supports infinite number of namespaces (0xFFFFFFFD)
    // Linux Kernel send identify namespace list for every 1024 ids
    *(uint32_t *)(buffer + 0x0204) = 1024;

    // Optional NVM Command Support
    {
      // [Bits ] Description
      // [15:06] Reserved
      // [05:05] 1 for Support reservations
      // [04:04] 1 for Support Save field in Set Features command and Select
      //         field in Get Features command
      // [03:03] 1 for Support Write Zeros command
      // [02:02] 1 for Support Dataset Management command
      // [01:01] 1 for Support Write Uncorrectable command
      // [00:00] 1 for Support Compare command
      buffer[0x0208] = 0x04;
      buffer[0x0209] = 0x00;
    }

    // Fused Operation Support
    {
      // [Bits ] Description
      // [15:01] Reserved
      // [00:00] 1 for Support Compare and Write fused operation
      buffer[0x020A] = 0x00;
      buffer[0x020B] = 0x00;
    }

    // Format NVM Attributes
    // [Bits ] Description
    // [07:03] Reserved
    // [02:02] 1 for Support cryptographic erase
    // [01:01] 1 for Support cryptographic erase performed on all namespaces,
    //         0 for namespace basis
    // [00:00] 1 for Format on specific namespace results on format on all
    //         namespaces, 0 for namespace basis
    buffer[0x020C] = 0x00;

    // Volatile Write Cache
    // [Bits ] Description
    // [07:01] Reserved
    // [00:00] 1 for volatile write cache is present
    buffer[0x020D] = 0x01;

    // Atomic Write Unit Normal
    {
      buffer[0x020E] = 0x00;
      buffer[0x020F] = 0x00;
    }

    // Atomic Write Unit Power Fail
    {
      buffer[0x0210] = 0x00;
      buffer[0x0211] = 0x00;
    }

    // NVM Vendor Specific Command Configuration
    // [Bits ] Description
    // [07:01] Reserved
    // [00:00] 1 for all vendor specific commands use the format at Figure 12.
    //         0 for format is vendor specific
    buffer[0x0212] = 0x00;

    // Reserved
    buffer[0x0213] = 0x00;

    // Atomic Compare & Write Unit
    {
      buffer[0x0214] = 0x00;
      buffer[0x0215] = 0x00;
    }

    // Reserved
    memset(buffer + 0x0216, 0, 2);

    // SGL Support
    {
      // [Bits ] Description
      // [31:21] Reserved
      // [20:20] 1 for Support Address field in SGL buffer Block
      // [19:19] 1 for Support MPTR containing SGL descriptor
      // [18:18] 1 for Support MPTR/DPTR containing SGL with larger than amount
      //         of buffer to be trasferred
      // [17:17] 1 for Support byte aligned contiguous physical buffer of
      //         metabuffer is supported
      // [16:16] 1 for Support SGL Bit Bucket descriptor
      // [15:03] Reserved
      // [02:02] 1 for Support Keyed SGL buffer Block descriptor
      // [01:01] Reserved
      // [00:00] 1 for Support SGLs in NVM Command Set
      buffer[0x0218] = 0x01;
      buffer[0x0219] = 0x00;
      buffer[0x021A] = 0x17;
      buffer[0x021B] = 0x00;
    }

    // Reserved
    memset(buffer + 0x021C, 0, 228);

    // NVM Subsystem NVMe Qualified Name
    {
      memset(buffer + 0x300, 0, 0x100);
      strncpy((char *)buffer + 0x0300,
              "nqn.2014-08.org.nvmexpress:uuid:270a1c70-962c-4116-6f1e340b9321",
              0x44);
    }

    // Reserved
    memset(buffer + 0x0400, 0, 768);

    // NVMe over Fabric
    memset(buffer + 0x0700, 0, 256);
  }

  /** Power State Descriptors **/
  // Power State 0
  /// Descriptor
  {
    // Maximum Power
    {
      buffer[0x0800] = 0xC4;
      buffer[0x0801] = 0x09;
    }

    // Reserved
    buffer[0x0802] = 0x00;

    // [Bits ] Description
    // [31:26] Reserved
    // [25:25] Non-Operational State
    // [24:24] Max Power Scale
    buffer[0x0803] = 0x00;

    // Entry Latency
    {
      buffer[0x0804] = 0x00;
      buffer[0x0805] = 0x00;
      buffer[0x0806] = 0x00;
      buffer[0x0807] = 0x00;
    }

    // Exit Latency
    {
      buffer[0x0808] = 0x00;
      buffer[0x0809] = 0x00;
      buffer[0x080A] = 0x00;
      buffer[0x080B] = 0x00;
    }

    // [Bits   ] Description
    // [103:101] Reserved
    // [100:096] Relative Read Throughput
    buffer[0x080C] = 0x00;

    // [Bits   ] Description
    // [111:109] Reserved
    // [108:104] Relative Read Latency
    buffer[0x080D] = 0x00;

    // [Bits   ] Description
    // [119:117] Reserved
    // [116:112] Relative Write Throughput
    buffer[0x080E] = 0x00;

    // [Bits   ] Description
    // [127:125] Reserved
    // [124:120] Relative Write Latency
    buffer[0x080E] = 0x00;

    // Idle Power
    {
      buffer[0x080F] = 0x00;
      buffer[0x0810] = 0x00;
    }

    // [Bits   ] Description
    // [151:150] Idle Power Scale
    // [149:144] Reserved
    buffer[0x0811] = 0x00;

    // Reserved
    buffer[0x0812] = 0x00;

    // Active Power
    {
      buffer[0x0813] = 0x00;
      buffer[0x0814] = 0x00;
    }

    // [Bits   ] Description
    // [183:182] Active Power Scale
    // [181:179] Reserved
    // [178:176] Active Power Workload
    buffer[0x0815] = 0x00;

    // Reserved
    memset(buffer + 0x0816, 0, 9);
  }

  // PSD1 ~ PSD31
  memset(buffer + 0x0820, 0, 992);

  // Vendor specific area
  memset(buffer + 0x0C00, 0, 1024);
}

void Identify::makeControllerList(CommandData *tag, ControllerID cntid,
                                  uint32_t nsid) {
  uint16_t size = 0;
  uint8_t *buffer = tag->buffer.data();

  if (nsid == NSID_ALL) {
    auto &controllerList = subsystem->getControllerList();

    for (auto &iter : controllerList) {
      if (iter.first >= cntid) {
        *(uint16_t *)(buffer + (++size) * 2) = iter.first;
      }
    }
  }
  else {
    auto &namespaceList = subsystem->getNamespaceList();
    auto ns = namespaceList.find(nsid);

    if (ns == namespaceList.end()) {
      tag->cqc->makeStatus(true, false, StatusType::GenericCommandStatus,
                           GenericCommandStatusCode::Invalid_Field);

      return;
    }

    auto &attachList = ns->second->getAttachment();

    for (auto &iter : attachList) {
      if (iter >= cntid) {
        *(uint16_t *)(buffer + (++size) * 2) = iter;
      }
    }
  }

  *(uint16_t *)(buffer) = size;
}

void Identify::dmaInitDone(uint64_t gcid) {
  auto tag = findTag(gcid);

  // Write buffer to host
  tag->dmaEngine->write(tag->request.getDMA(), 0, 4096, tag->buffer.data(),
                        NoMemoryAccess, dmaCompleteEvent, gcid);
}

void Identify::dmaComplete(uint64_t gcid) {
  auto tag = findTag(gcid);

  subsystem->complete(tag);
}

void Identify::setRequest(ControllerData *cdata, AbstractNamespace *,
                          SQContext *req) {
  auto tag = createTag(cdata, req);
  auto entry = req->getData();

  // Get parameters
  uint32_t nsid = entry->namespaceID;
  uint8_t cns = entry->dword10 & 0xFF;
  uint16_t cntid = (entry->dword10 & 0xFFFF0000) >> 16;
  uint8_t csi = entry->dword11 >> 24;
  uint16_t setid = entry->dword11 & 0xFFFF;
  uint8_t uuid = entry->dword14 & 0x7F;

  debugprint_command(tag,
                     "ADMIN   | Identify | CNS %u | CNTID %u | CSI %u | NSID "
                     "%u | NVMSET %u | UUID %u",
                     cns, cntid, csi, nsid, setid, uuid);

  // Make response
  tag->createResponse();

  if (csi > 2) {
    tag->cqc->makeStatus(false, false, StatusType::CommandSpecificStatus,
                         CommandSpecificStatusCode::Invalid_IOCommandSet);

    goto end;
  }

  // Make buffer
  tag->buffer.resize(4096);

  switch ((IdentifyStructure)cns) {
    case IdentifyStructure::Namespace:
      makeNamespaceStructure(tag, CommandSetIdentifier::NVM, nsid);

      break;
    case IdentifyStructure::IOCommandSetSpecificNamespace:
      if (csi > 0) {
        makeNamespaceStructure(tag, (CommandSetIdentifier)csi, nsid);
      }

      break;
    case IdentifyStructure::Controller:
      makeControllerStructure(tag, CommandSetIdentifier::NVM);

      break;
    case IdentifyStructure::IOCommandSetSpecificController:
      if (csi > 0) {
        makeControllerStructure(tag, (CommandSetIdentifier)csi);
      }

      break;
    case IdentifyStructure::ActiveNamespaceList:
      makeNamespaceList(tag, CommandSetIdentifier::NVM, nsid);

      break;
    case IdentifyStructure::IOCommandSetSpecificActiveNamespaceList:
      makeNamespaceList(tag, (CommandSetIdentifier)csi, nsid);

      break;
    case IdentifyStructure::NamespaceIdentificationDescriptorList:
      makeNamespaceDescriptor(tag, nsid);

      break;
    case IdentifyStructure::NVMSetList:
      break;
    case IdentifyStructure::AllocatedNamespaceList:
      makeNamespaceList(tag, CommandSetIdentifier::NVM, nsid, true);

      break;
    case IdentifyStructure::IOCommandSetSpecificAllocatedNamespaceList:
      makeNamespaceList(tag, (CommandSetIdentifier)csi, nsid, true);

      break;
    case IdentifyStructure::AllocatedNamespace:
      makeNamespaceStructure(tag, CommandSetIdentifier::NVM, nsid, true);

      break;
    case IdentifyStructure::IOCommandSetSpecificAllocatedNamespace:
      if (csi > 0) {
        makeNamespaceStructure(tag, (CommandSetIdentifier)csi, nsid, true);
      }

      break;
    case IdentifyStructure::AttachedControllerList:
      makeControllerList(tag, cntid, nsid);

      break;
    case IdentifyStructure::ControllerList:
      makeControllerList(tag, cntid);

      break;
    case IdentifyStructure::IOCommandSet:
      // SimpleSSD always support any I/O command set combination
      tag->buffer[0] = 0x07;

      break;
    case IdentifyStructure::PrimaryControllerCapabilities:
    case IdentifyStructure::SecondaryControllerList:
    case IdentifyStructure::NamespaceGranularityList:
    case IdentifyStructure::UUIDList:
    default:
      tag->cqc->makeStatus(true, false, StatusType::GenericCommandStatus,
                           GenericCommandStatusCode::Invalid_Field);

      break;
  }

end:
  if (tag->cqc->isSuccess()) {
    // Data generated successfully. DMA data
    tag->createDMAEngine(4096, dmaInitEvent);
  }
  else {
    // Complete immediately
    subsystem->complete(tag);
  }
}

void Identify::getStatList(std::vector<Stat> &, std::string) noexcept {}

void Identify::getStatValues(std::vector<double> &) noexcept {}

void Identify::resetStatValues() noexcept {}

void Identify::createCheckpoint(std::ostream &out) const noexcept {
  Command::createCheckpoint(out);

  BACKUP_EVENT(out, dmaInitEvent);
  BACKUP_EVENT(out, dmaCompleteEvent);
}

void Identify::restoreCheckpoint(std::istream &in) noexcept {
  Command::restoreCheckpoint(in);

  RESTORE_EVENT(in, dmaInitEvent);
  RESTORE_EVENT(in, dmaCompleteEvent);
}

}  // namespace SimpleSSD::HIL::NVMe
