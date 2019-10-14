// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "hil/nvme/command/identify.hh"

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

void Identify::makeNamespaceStructure(IOCommandData *tag, uint32_t nsid,
                                      bool force) {
  auto ctrlID = tag->controller->getControllerID();

  if (nsid == NSID_ALL) {
    // We support Namespace Management, so return common namespace info,
    // especially LBA format information.

    // Number of LBA Formats
    tag->buffer[25] = nLBAFormat - 1;  // 0's based

    // LBA Formats
    memcpy(tag->buffer + 128, lbaFormat, 4 * nLBAFormat);
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
      auto pHIL = subsystem->getHIL();
      auto logicalPageSize = subsystem->getLPNSize();

      // Namespace Size
      memcpy(tag->buffer + 0, &info->size, 8);

      // Namespace Capacity
      memcpy(tag->buffer + 8, &info->capacity, 8);

      // Namespace Utilization
      info->utilization = pHIL->getPageUsage(info->namespaceRange.first,
                                             info->namespaceRange.second);
      info->utilization *= logicalPageSize;
      info->utilization /= info->lbaSize;

      memcpy(tag->buffer + 16, &info->utilization, 8);

      // Namespace Features
      tag->buffer[24] = 0x04;  // Trim supported

      // Number of LBA Formats
      tag->buffer[25] = nLBAFormat - 1;  // 0's based

      // Formatted LBA Size
      tag->buffer[26] = info->lbaFormatIndex;

      // End-to-end Data Protection Capabilities
      tag->buffer[28] = info->dataProtectionSettings;

      // Namespace Multi-path I/O and Namespace Sharing Capabilities
      tag->buffer[30] = info->namespaceSharingCapabilities;

      // NVM capacity
      memcpy(tag->buffer + 48, &info->sizeInByteL, 8);
      memcpy(tag->buffer + 56, &info->sizeInByteH, 8);

      // LBA Formats
      memcpy(tag->buffer + 128, lbaFormat, 4 * nLBAFormat);
    }
    else {
      // Namespace not attached
      tag->cqc->makeStatus(true, false, StatusType::CommandSpecificStatus,
                           CommandSpecificStatusCode::NamespaceNotAttached);
    }
  }
}

void Identify::makeNamespaceList(IOCommandData *tag, uint32_t nsid,
                                 bool force) {
  uint16_t idx = 0;

  if (nsid >= NSID_ALL - 1) {
    // Invalid namespace ID
    tag->cqc->makeStatus(true, false, StatusType::GenericCommandStatus,
                         GenericCommandStatusCode::Invalid_Field);
  }
  else {
    if (force) {
      auto &namespaceList = subsystem->getNamespaceList();

      for (auto &iter : namespaceList) {
        ((uint32_t *)tag->buffer)[idx++] = iter.first;
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
        ((uint32_t *)tag->buffer)[idx++] = *iter;
      }
    }
  }
}

void Identify::makeControllerStructure(IOCommandData *tag) {
  uint16_t vid, ssvid;
  uint16_t id;
  uint64_t totalSize;
  uint64_t unallocated;
  uint64_t lpnSize;

  tag->interface->getPCIID(vid, ssvid);

  id = (uint16_t)tag->controller->getControllerID();

  lpnSize = subsystem->getLPNSize();
  totalSize = subsystem->getTotalPages() * lpnSize;
  unallocated = subsystem->getAllocatedPages() * lpnSize;

  unallocated = totalSize - unallocated;

  /** Controller Capabilities and Features **/
  {
    // PCI Vendor ID
    memcpy(tag->buffer + 0x0000, &vid, 2);

    // PCI Subsystem Vendor ID
    memcpy(tag->buffer + 0x0002, &ssvid, 2);

    // Serial Number
    memcpy(tag->buffer + 0x0004, "00000000000000000000", 0x14);

    // Model Number
    memcpy(tag->buffer + 0x0018, "SimpleSSD NVMe Controller by CAMELab    ",
           0x28);

    // Firmware Revision
    memcpy(tag->buffer + 0x0040, "03.00.00", 0x08);

    // Recommended Arbitration Burst
    tag->buffer[0x0048] = 0x00;

    // IEEE OUI Identifier (Same as Inter 750)
    {
      tag->buffer[0x0049] = 0xE4;
      tag->buffer[0x004A] = 0xD2;
      tag->buffer[0x004B] = 0x5C;
    }

    // Controller Multi-Path I/O and Namespace Sharing Capabilities
    // [Bits ] Description
    // [07:03] Reserved
    // [02:02] 1 for SR-IOV Virtual Function, 0 for PCI (Physical) Function
    // [01:01] 1 for more than one host may connected to NVM subsystem
    // [00:00] 1 for NVM subsystem may has more than one NVM subsystem port
    tag->buffer[0x004C] = 0x02;

    // Maximum tag->buffer Transfer Size
    tag->buffer[0x004D] = 0x00;  // No limit

    // Controller ID
    { *(uint16_t *)(tag->buffer + 0x004E) = (uint16_t)id; }

    // Version
    {
      tag->buffer[0x0050] = 0x00;
      tag->buffer[0x0051] = 0x03;
      tag->buffer[0x0052] = 0x01;
      tag->buffer[0x0053] = 0x00;
    }  // NVM Express 1.3 Compliant Controller

    // RTD3 Resume Latency
    {
      tag->buffer[0x0054] = 0x00;
      tag->buffer[0x0055] = 0x00;
      tag->buffer[0x0056] = 0x00;
      tag->buffer[0x0057] = 0x00;
    }  // Not reported

    // RTD3 Enter Latency
    {
      tag->buffer[0x0058] = 0x00;
      tag->buffer[0x0059] = 0x00;
      tag->buffer[0x005A] = 0x00;
      tag->buffer[0x005B] = 0x00;
    }  // Not repotred

    // Optional Asynchronous Events Supported
    {
      // [Bits ] Description
      // [31:10] Reserved
      // [09:09] 1 for Support Firmware Activation Notice
      // [08:08] 1 for Support Namespace Attributes Notice
      // [07:00] Reserved
      tag->buffer[0x005C] = 0x00;
      tag->buffer[0x005D] = 0x00;
      tag->buffer[0x005E] = 0x00;
      tag->buffer[0x005F] = 0x00;
    }

    // Controller Attributes
    {
      // [Bits ] Description
      // [31:01] Reserved
      // [00:00] 1 for Support 128-bit Host Identifier
      tag->buffer[0x0060] = 0x00;
      tag->buffer[0x0061] = 0x00;
      tag->buffer[0x0062] = 0x00;
      tag->buffer[0x0063] = 0x00;
    }
    memset(tag->buffer + 0x0064, 0, 156);  // Reserved
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
      tag->buffer[0x0100] = 0x0A;
      tag->buffer[0x0101] = 0x00;
    }

    // Abort Command Limit
    tag->buffer[0x0102] = 0x03;  // Recommanded value is 4 (3 + 1)

    // Asynchronous Event Request Limit
    tag->buffer[0x0103] = 0x03;  // Recommanded value is 4 (3 + 1))

    // Firmware Updates
    // [Bits ] Description
    // [07:05] Reserved
    // [04:04] 1 for Support firmware activation without a reset
    // [03:01] The number of firmware slot
    // [00:00] 1 for First firmware slot is read only, 0 for read/write
    tag->buffer[0x0104] = 0x00;

    // Log Page Attributes
    // [Bits ] Description
    // [07:03] Reserved
    // [02:02] 1 for Support extended tag->buffer for Get Log Page command
    // [01:01] 1 for Support Command Effects log page
    // [00:00] 1 for Support S.M.A.R.T. / Health information log page per
    //         namespace basis
    tag->buffer[0x0105] = 0x01;

    // Error Log Page Entries, 0's based value
    tag->buffer[0x0106] = 0x63;  // 64 entries

    // Number of Power States Support, 0's based value
    tag->buffer[0x0107] = 0x00;  // 1 states

    // Admin Vendor Specific Command Configuration
    // [Bits ] Description
    // [07:01] Reserved
    // [00:00] 1 for all vendor specific commands use the format at Figure 12.
    //         0 for format is vendor specific
    tag->buffer[0x0108] = 0x00;

    // Autonomous Power State Transition Attributes
    // [Bits ] Description
    // [07:01] Reserved
    // [00:00] 1 for Support autonomous power state transitions
    tag->buffer[0x0109] = 0x00;

    // Warning Composite Temperature Threshold
    {
      tag->buffer[0x010A] = 0x00;
      tag->buffer[0x010B] = 0x00;
    }

    // Critical Composite Temperature Threshold
    {
      tag->buffer[0x010C] = 0x00;
      tag->buffer[0x010D] = 0x00;
    }

    // Maximum Time for Firmware Activation
    {
      tag->buffer[0x010E] = 0x00;
      tag->buffer[0x010F] = 0x00;
    }

    // Host Memory Buffer Preferred Size
    {
      tag->buffer[0x0110] = 0x00;
      tag->buffer[0x0111] = 0x00;
      tag->buffer[0x0112] = 0x00;
      tag->buffer[0x0113] = 0x00;
    }

    // Host Memory Buffer Minimum Size
    {
      tag->buffer[0x0114] = 0x00;
      tag->buffer[0x0115] = 0x00;
      tag->buffer[0x0116] = 0x00;
      tag->buffer[0x0117] = 0x00;
    }

    // Total NVM Capacity
    {
      memcpy(tag->buffer + 0x118, &totalSize, 8);
      memset(tag->buffer + 0x120, 0, 8);
    }

    // Unallocated NVM Capacity
    {
      memcpy(tag->buffer + 0x118, &unallocated, 8);
      memset(tag->buffer + 0x120, 0, 8);
    }

    // Replay Protected Memory Block Support
    {
      // [Bits ] Description
      // [31:24] Access Size
      // [23:16] Total Size
      // [15:06] Reserved
      // [05:03] Authentication Method
      // [02:00] Number of RPMB Units
      tag->buffer[0x0138] = 0x00;
      tag->buffer[0x0139] = 0x00;
      tag->buffer[0x013A] = 0x00;
      tag->buffer[0x013B] = 0x00;
    }

    // Reserved
    memset(tag->buffer + 0x013C, 0, 4);

    // Keep Alive Support
    {
      tag->buffer[0x0140] = 0x00;
      tag->buffer[0x0141] = 0x00;
    }

    // Reserved
    memset(tag->buffer + 0x0142, 0, 190);
  }

  /** NVM Command Set Attributes **/
  {
    // Submission Queue Entry Size
    // [Bits ] Description
    // [07:04] Maximum Submission Queue Entry Size
    // [03:00] Minimum Submission Queue Entry Size
    tag->buffer[0x0200] = 0x66;  // 64Bytes, 64Bytes

    // Completion Queue Entry Size
    // [Bits ] Description
    // [07:04] Maximum Completion Queue Entry Size
    // [03:00] Minimum Completion Queue Entry Size
    tag->buffer[0x0201] = 0x44;  // 16Bytes, 16Bytes

    // Maximum Outstanding Commands
    {
      tag->buffer[0x0202] = 0x00;
      tag->buffer[0x0203] = 0x00;
    }

    // Number of Namespaces
    // SimpleSSD supports infinite number of namespaces (0xFFFFFFFD)
    // Linux Kernel send identify namespace list for every 1024 ids
    *(uint32_t *)(tag->buffer + 0x0204) = 1024;

    // Optional NVM Command Support
    {
      // [Bits ] Description
      // [15:06] Reserved
      // [05:05] 1 for Support reservations
      // [04:04] 1 for Support Save field in Set Features command and Select
      //         field in Get Features command
      // [03:03] 1 for Support Write Zeros command
      // [02:02] 1 for Support tag->bufferset Management command
      // [01:01] 1 for Support Write Uncorrectable command
      // [00:00] 1 for Support Compare command
      tag->buffer[0x0208] = 0x04;
      tag->buffer[0x0209] = 0x00;
    }

    // Fused Operation Support
    {
      // [Bits ] Description
      // [15:01] Reserved
      // [00:00] 1 for Support Compare and Write fused operation
      tag->buffer[0x020A] = 0x00;
      tag->buffer[0x020B] = 0x00;
    }

    // Format NVM Attributes
    // [Bits ] Description
    // [07:03] Reserved
    // [02:02] 1 for Support cryptographic erase
    // [01:01] 1 for Support cryptographic erase performed on all namespaces,
    //         0 for namespace basis
    // [00:00] 1 for Format on specific namespace results on format on all
    //         namespaces, 0 for namespace basis
    tag->buffer[0x020C] = 0x00;

    // Volatile Write Cache
    // [Bits ] Description
    // [07:01] Reserved
    // [00:00] 1 for volatile write cache is present

    // TODO: FILL HERE!
    // tag->buffer[0x020D] = readConfigBoolean(Section::InternalCache,
    //                                    ICL::Config::Key::UseWriteCache)
    //                      ? 0x01
    //                      : 0x00;

    // Atomic Write Unit Normal
    {
      tag->buffer[0x020E] = 0x00;
      tag->buffer[0x020F] = 0x00;
    }

    // Atomic Write Unit Power Fail
    {
      tag->buffer[0x0210] = 0x00;
      tag->buffer[0x0211] = 0x00;
    }

    // NVM Vendor Specific Command Configuration
    // [Bits ] Description
    // [07:01] Reserved
    // [00:00] 1 for all vendor specific commands use the format at Figure 12.
    //         0 for format is vendor specific
    tag->buffer[0x0212] = 0x00;

    // Reserved
    tag->buffer[0x0213] = 0x00;

    // Atomic Compare & Write Unit
    {
      tag->buffer[0x0214] = 0x00;
      tag->buffer[0x0215] = 0x00;
    }

    // Reserved
    memset(tag->buffer + 0x0216, 0, 2);

    // SGL Support
    {
      // [Bits ] Description
      // [31:21] Reserved
      // [20:20] 1 for Support Address field in SGL tag->buffer Block
      // [19:19] 1 for Support MPTR containing SGL descriptor
      // [18:18] 1 for Support MPTR/DPTR containing SGL with larger than amount
      //         of tag->buffer to be trasferred
      // [17:17] 1 for Support byte aligned contiguous physical tag->buffer of
      //         metatag->buffer is supported
      // [16:16] 1 for Support SGL Bit Bucket descriptor
      // [15:03] Reserved
      // [02:02] 1 for Support Keyed SGL tag->buffer Block descriptor
      // [01:01] Reserved
      // [00:00] 1 for Support SGLs in NVM Command Set
      tag->buffer[0x0218] = 0x00;  // Disable SGL for remote NVMe interface
      tag->buffer[0x0219] = 0x00;
      tag->buffer[0x021A] = 0x17;
      tag->buffer[0x021B] = 0x00;
    }

    // Reserved
    memset(tag->buffer + 0x021C, 0, 228);

    // NVM Subsystem NVMe Qualified Name
    {
      memset(tag->buffer + 0x300, 0, 0x100);
      strncpy((char *)tag->buffer + 0x0300,
              "nqn.2014-08.org.nvmexpress:uuid:270a1c70-962c-4116-6f1e340b9321",
              0x44);
    }

    // Reserved
    memset(tag->buffer + 0x0400, 0, 768);

    // NVMe over Fabric
    memset(tag->buffer + 0x0700, 0, 256);
  }

  /** Power State Descriptors **/
  // Power State 0
  /// Descriptor
  {
    // Maximum Power
    {
      tag->buffer[0x0800] = 0xC4;
      tag->buffer[0x0801] = 0x09;
    }

    // Reserved
    tag->buffer[0x0802] = 0x00;

    // [Bits ] Description
    // [31:26] Reserved
    // [25:25] Non-Operational State
    // [24:24] Max Power Scale
    tag->buffer[0x0803] = 0x00;

    // Entry Latency
    {
      tag->buffer[0x0804] = 0x00;
      tag->buffer[0x0805] = 0x00;
      tag->buffer[0x0806] = 0x00;
      tag->buffer[0x0807] = 0x00;
    }

    // Exit Latency
    {
      tag->buffer[0x0808] = 0x00;
      tag->buffer[0x0809] = 0x00;
      tag->buffer[0x080A] = 0x00;
      tag->buffer[0x080B] = 0x00;
    }

    // [Bits   ] Description
    // [103:101] Reserved
    // [100:096] Relative Read Throughput
    tag->buffer[0x080C] = 0x00;

    // [Bits   ] Description
    // [111:109] Reserved
    // [108:104] Relative Read Latency
    tag->buffer[0x080D] = 0x00;

    // [Bits   ] Description
    // [119:117] Reserved
    // [116:112] Relative Write Throughput
    tag->buffer[0x080E] = 0x00;

    // [Bits   ] Description
    // [127:125] Reserved
    // [124:120] Relative Write Latency
    tag->buffer[0x080E] = 0x00;

    // Idle Power
    {
      tag->buffer[0x080F] = 0x00;
      tag->buffer[0x0810] = 0x00;
    }

    // [Bits   ] Description
    // [151:150] Idle Power Scale
    // [149:144] Reserved
    tag->buffer[0x0811] = 0x00;

    // Reserved
    tag->buffer[0x0812] = 0x00;

    // Active Power
    {
      tag->buffer[0x0813] = 0x00;
      tag->buffer[0x0814] = 0x00;
    }

    // [Bits   ] Description
    // [183:182] Active Power Scale
    // [181:179] Reserved
    // [178:176] Active Power Workload
    tag->buffer[0x0815] = 0x00;

    // Reserved
    memset(tag->buffer + 0x0816, 0, 9);
  }

  // PSD1 ~ PSD31
  memset(tag->buffer + 0x0820, 0, 992);

  // Vendor specific area
  memset(tag->buffer + 0x0C00, 0, 1024);
}

void Identify::makeControllerList(IOCommandData *tag, ControllerID cntid,
                                  uint32_t nsid) {
  uint16_t size = 0;

  if (nsid == NSID_ALL) {
    auto &controllerList = subsystem->getControllerList();

    for (auto &iter : controllerList) {
      if (iter.first >= cntid) {
        *(uint16_t *)(tag->buffer + (++size) * 2) = iter.first;
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
        *(uint16_t *)(tag->buffer + (++size) * 2) = iter;
      }
    }
  }

  *(uint16_t *)(tag->buffer) = size;
}

void Identify::dmaInitDone(uint64_t gcid) {
  auto tag = findIOTag(gcid);

  // Write tag->buffer to host
  tag->dmaEngine->write(tag->dmaTag, 0, 4096, tag->buffer, dmaCompleteEvent,
                        gcid);
}

void Identify::dmaComplete(uint64_t gcid) {
  auto tag = findIOTag(gcid);

  subsystem->complete(tag);
}

void Identify::setRequest(ControllerData *cdata, SQContext *req) {
  auto tag = createIOTag(cdata, req);
  auto entry = req->getData();

  // Get parameters
  uint32_t nsid = entry->namespaceID;
  uint8_t cns = entry->dword10 & 0xFF;
  uint16_t cntid = (entry->dword10 & 0xFFFF0000) >> 16;
  uint16_t setid = entry->dword11 & 0xFFFF;
  uint8_t uuid = entry->dword14 & 0x7F;

  debugprint_command(
      tag,
      "ADMIN   | Identify | CNS %u | CNTID %u | NSID %u | NVMSET %u | UUID %u",
      cns, cntid, nsid, setid, uuid);

  // Make response
  tag->createResponse();

  // Make buffer
  tag->buffer = (uint8_t *)calloc(4096, 1);

  switch ((IdentifyStructure)cns) {
    case IdentifyStructure::Namespace:
      makeNamespaceStructure(tag, nsid);

      break;
    case IdentifyStructure::Controller:
      makeControllerStructure(tag);

      break;
    case IdentifyStructure::ActiveNamespaceList:
      makeNamespaceList(tag, nsid);

      break;
    case IdentifyStructure::NamespaceIdentificationDescriptorList:
      // Return zero-filled data

      break;
    case IdentifyStructure::NVMSetList:
      break;
    case IdentifyStructure::AllocatedNamespaceList:
      makeNamespaceList(tag, nsid, true);

      break;
    case IdentifyStructure::AllocatedNamespace:
      makeNamespaceStructure(tag, nsid, true);

      break;
    case IdentifyStructure::AttachedControllerList:
      makeControllerList(tag, cntid, nsid);

      break;
    case IdentifyStructure::ControllerList:
      makeControllerList(tag, cntid);

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

  if (tag->cqc->isSuccess()) {
    // Data generated successfully. DMA data
    tag->createDMAEngine(4096, dmaInitEvent);
  }
  else {
    // Complete immediately
    subsystem->complete(tag);
  }
}

void Identify::completeRequest(CommandTag tag) {
  if (((IOCommandData *)tag)->buffer) {
    free(((IOCommandData *)tag)->buffer);
  }
  if (((IOCommandData *)tag)->dmaTag != InvalidDMATag) {
    tag->dmaEngine->deinit(((IOCommandData *)tag)->dmaTag);
  }

  destroyTag(tag);
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
