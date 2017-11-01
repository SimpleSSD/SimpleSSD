/*
 * Copyright (C) 2017 CAMELab
 *
 * This file is part of SimpleSSD.
 *
 * SimpleSSD is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * SimpleSSD is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with SimpleSSD.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "hil/nvme/controller.hh"

#include <cmath>

#include "hil/nvme/subsystem.hh"

namespace SimpleSSD {

namespace HIL {

namespace NVMe {

RegisterTable::_RegisterTable() {
  memset(data, 0, 64);
}

Controller::Controller(Interface *intrface, ConfigReader *c)
    : pParent(intrface),
      adminQueueInited(false),
      interruptMask(0),
      conf(c->nvmeConfig) {
  pDmaEngine = new DMAScheduler(pParent, &conf);

  // Allocate array for Command Queues
  ppCQueue = (CQueue **)calloc(conf.readUint(NVME_MAX_IO_CQUEUE) + 1,
                               sizeof(CQueue *));
  ppSQueue = (SQueue **)calloc(conf.readUint(NVME_MAX_IO_SQUEUE) + 1,
                               sizeof(SQueue *));

  // [Bits ] Name  : Description                     : Current Setting
  // [63:56] Reserved
  // [55:52] MPSMZX: Memory Page Size Maximum        : 2^14 Bytes
  // [51:48] MPSMIN: Memory Page Size Minimum        : 2^12 Bytes
  // [47:45] Reserved
  // [44:37] CSS   : Command Sets Supported          : NVM command set
  // [36:36] NSSRS : NVM Subsystem Reset Supported   : No
  // [35:32] DSTRD : Doorbell Stride                 : 0 (4 bytes)
  // [31:24] TO    : Timeout                         : 40 * 500ms
  // [23:19] Reserved
  // [18:17] AMS   : Arbitration Mechanism Supported : Weighted Round Robin
  // [16:16] CQR   : Contiguous Queues Required      : Yes
  // [15:00] MQES  : Maximum Queue Entries Supported : 4096 Entries
  registers.capabilities = 0x0020002028010FFF;
  registers.version = 0x00010201;  // NVMe 1.2.1

  cfgdata.pConfigReader = c;
  cfgdata.pDmaEngine = pDmaEngine;
  cfgdata.maxQueueEntry = (registers.capabilities & 0xFFFF) + 1;

  pSubsystem = new Subsystem(this, &cfgdata);
}

Controller::~Controller() {
  delete pSubsystem;

  for (uint16_t i = 0; i < conf.readUint(NVME_MAX_IO_CQUEUE) + 1; i++) {
    if (ppCQueue[i]) {
      delete ppCQueue[i];
    }
  }

  for (uint16_t i = 0; i < conf.readUint(NVME_MAX_IO_SQUEUE) + 1; i++) {
    if (ppSQueue[i]) {
      delete ppSQueue[i];
    }
  }

  free(ppCQueue);
  free(ppSQueue);
}

void Controller::readRegister(uint64_t offset, uint64_t size, uint8_t *buffer,
                              uint64_t &tick) {
  registers.interruptMaskSet = interruptMask;
  registers.interruptMaskClear = interruptMask;

  memcpy(buffer, registers.data + offset, size);
  pDmaEngine->read(0, size, nullptr, tick);

  /*
  TODO:
  switch (offset) {
    case REG_CONTROLLER_CAPABILITY:
    case REG_CONTROLLER_CAPABILITY + 4:   DPRINTF(NVMeAll, "BAR0    | READ  |
  Controller Capabilities\n");              break; case REG_VERSION:
  DPRINTF(NVMeAll, "BAR0    | READ  | Version\n"); break; case
  REG_INTERRUPT_MASK_SET:          DPRINTF(NVMeAll, "BAR0    | READ  | Interrupt
  Mask Set\n");                   break; case REG_INTERRUPT_MASK_CLEAR:
  DPRINTF(NVMeAll, "BAR0    | READ  | Interrupt Mask Clear\n"); break; case
  REG_CONTROLLER_CONFIG:           DPRINTF(NVMeAll, "BAR0    | READ  |
  Controller Configuration\n");             break; case REG_CONTROLLER_STATUS:
  DPRINTF(NVMeAll, "BAR0    | READ  | Controller Status\n"); break; case
  REG_NVM_SUBSYSTEM_RESET:         DPRINTF(NVMeAll, "BAR0    | READ  | NVM
  Subsystem Reset\n");                  break; case REG_ADMIN_QUEUE_ATTRIBUTE:
  DPRINTF(NVMeAll, "BAR0    | READ  | Admin Queue Attributes\n"); break; case
  REG_ADMIN_SQUEUE_BASE_ADDR: case REG_ADMIN_SQUEUE_BASE_ADDR + 4:
  DPRINTF(NVMeAll, "BAR0    | READ  | Admin Submission Queue Base Address\n");
  break; case REG_ADMIN_CQUEUE_BASE_ADDR: case REG_ADMIN_CQUEUE_BASE_ADDR + 4:
  DPRINTF(NVMeAll, "BAR0    | READ  | Admin Completion Queue Base Address\n");
  break; case REG_CMB_LOCATION:                DPRINTF(NVMeAll, "BAR0    | READ
  | Controller Memory Buffer Location\n");    break; case REG_CMB_SIZE:
  DPRINTF(NVMeAll, "BAR0    | READ  | Controller Memory Buffer Size\n"); break;
  }

  if (size == 4) {
    DPRINTF(NVMeDMA, "DMAPORT | READ  | DATA %08" PRIX32 "\n", *(uint32_t
  *)buffer);
  }
  else {
    DPRINTF(NVMeDMA, "DMAPORT | READ  | DATA %016" PRIX64 "\n", *(uint64_t
  *)buffer);
  }
  */
}

void Controller::writeRegister(uint64_t offset, uint64_t size, uint8_t *buffer,
                               uint64_t &tick) {
  uint32_t uiTemp32;
  uint64_t uiTemp64;

  pDmaEngine->write(0, size, nullptr, tick);

  if (size == 4) {
    memcpy(&uiTemp32, buffer, 4);

    switch (offset) {
      case REG_INTERRUPT_MASK_SET:
        // DPRINTF(NVMeAll, "BAR0    | WRITE | Interrupt Mask Set\n");

        interruptMask |= uiTemp32;

        break;
      case REG_INTERRUPT_MASK_CLEAR:
        // DPRINTF(NVMeAll, "BAR0    | WRITE | Interrupt Mask Clear\n");

        interruptMask &= ~uiTemp32;

        break;
      case REG_CONTROLLER_CONFIG:
        // DPRINTF(NVMeAll, "BAR0    | WRITE | Controller Configuration\n");

        registers.configuration &= 0xFF00000E;
        registers.configuration |= (uiTemp32 & 0x00FFFFF1);

        // Update entry size
        sqstride = (int)powf(2.f, (registers.configuration & 0x000F0000) >> 16);
        cqstride = (int)powf(2.f, (registers.configuration & 0x00F00000) >> 20);

        // Update Memory Page Size
        cfgdata.memoryPageSizeOrder =
            ((registers.configuration & 0x780) >> 7) + 11;  // CC.MPS + 12 - 1
        cfgdata.memoryPageSize =
            (int)powf(2.f, cfgdata.memoryPageSizeOrder + 1);

        // Update Arbitration Mechanism
        arbitration = (registers.configuration & 0x00003800) >> 11;

        // Apply to admin queue
        if (ppCQueue[0]) {
          ppCQueue[0]->setBase(
              new PRPList(&cfgdata, registers.adminCQueueBaseAddress,
                          ppCQueue[0]->getSize() * cqstride, true),
              cqstride);
        }
        if (ppSQueue[0]) {
          ppSQueue[0]->setBase(
              new PRPList(&cfgdata, registers.adminSQueueBaseAddress,
                          ppSQueue[0]->getSize() * sqstride, true),
              sqstride);
        }

        // If EN = 1, Set CSTS.RDY = 1
        if (registers.configuration & 0x00000001) {
          registers.status |= 0x00000001;

          pParent->enableController(conf.readUint(NVME_QUEUE_INTERVAL),
                                    conf.readUint(NVME_WORK_INTERVAL));
        }
        // If EN = 0, Set CSTS.RDY = 0
        else {
          registers.status &= 0xFFFFFFFE;

          pParent->disableController();
        }

        break;
      case REG_CONTROLLER_STATUS:
        // DPRINTF(NVMeAll, "BAR0    | WRITE | Controller Status\n");

        // Clear NSSRO if set
        if (uiTemp32 & 0x00000010) {
          registers.status &= 0xFFFFFFEF;
        }

        break;
      case REG_NVM_SUBSYSTEM_RESET:
        // DPRINTF(NVMeAll, "BAR0    | WRITE | NVM Subsystem Reset\n");

        registers.subsystemReset = uiTemp32;

        // FIXME: If NSSR is same as NVMe(0x4E564D65), do NVMe Subsystem reset
        // (when CAP.NSSRS is 1)
        break;
      case REG_ADMIN_QUEUE_ATTRIBUTE:
        // DPRINTF(NVMeAll, "BAR0    | WRITE | Admin Queue Attributes\n");

        registers.adminQueueAttributes &= 0xF000F000;
        registers.adminQueueAttributes |= (uiTemp32 & 0x0FFF0FFF);

        break;
      case REG_ADMIN_CQUEUE_BASE_ADDR:
        // DPRINTF(NVMeAll, "BAR0    | WRITE | Admin Completion Queue Base
        // Address | L\n");

        memcpy(&(registers.adminCQueueBaseAddress), buffer, 4);
        adminQueueInited++;

        break;
      case REG_ADMIN_CQUEUE_BASE_ADDR + 4:
        // DPRINTF(NVMeAll, "BAR0    | WRITE | Admin Completion Queue Base
        // Address | H\n");

        memcpy(((uint8_t *)&(registers.adminCQueueBaseAddress)) + 4, buffer, 4);
        adminQueueInited++;

        break;
      case REG_ADMIN_SQUEUE_BASE_ADDR:
        // DPRINTF(NVMeAll, "BAR0    | WRITE | Admin Submission Queue Base
        // Address | L\n");
        memcpy(&(registers.adminSQueueBaseAddress), buffer, 4);
        adminQueueInited++;

        break;
      case REG_ADMIN_SQUEUE_BASE_ADDR + 4:
        // DPRINTF(NVMeAll, "BAR0    | WRITE | Admin Submission Queue Base
        // Address | H\n");
        memcpy(((uint8_t *)&(registers.adminSQueueBaseAddress)) + 4, buffer, 4);
        adminQueueInited++;

        break;
      default:
        // panic("nvme_ctrl: Write on read only register\n");
        break;
    }

    // DPRINTF(NVMeDMA, "DMAPORT | WRITE | DATA %08" PRIX32 "\n", uiTemp32);
  }
  else if (size == 8) {
    memcpy(&uiTemp64, buffer, 8);

    switch (offset) {
      case REG_ADMIN_CQUEUE_BASE_ADDR:
        // DPRINTF(NVMeAll, "BAR0    | WRITE | Admin Completion Queue Base
        // Address\n");

        registers.adminCQueueBaseAddress = uiTemp64;
        adminQueueInited += 2;

        break;
      case REG_ADMIN_SQUEUE_BASE_ADDR:
        // DPRINTF(NVMeAll, "BAR0    | WRITE | Admin Submission Queue Base
        // Address\n");

        registers.adminSQueueBaseAddress = uiTemp64;
        adminQueueInited += 2;

        break;
      default:
        // panic("nvme_ctrl: Write on read only register\n");
        break;
    }

    // DPRINTF(NVMeDMA, "DMAPORT | WRITE | DATA %016" PRIX64 "\n", uiTemp64);
  }
  else {
    // panic("nvme_ctrl: Invalid read size(%d) on controller register\n", size);
  }

  if (adminQueueInited == 4) {
    uint16_t entrySize = 0;

    adminQueueInited = 0;

    entrySize = ((registers.adminQueueAttributes & 0x0FFF0000) >> 16) + 1;
    ppCQueue[0] = new CQueue(0, true, 0, entrySize);

    // DPRINTF(NVMeQueue, "CQ 0    | CREATE | Entry size %d\n", entrySize);

    entrySize = (registers.adminQueueAttributes & 0x0FFF) + 1;
    ppSQueue[0] = new SQueue(0, 0, 0, entrySize);

    // DPRINTF(NVMeQueue, "SQ 0    | CREATE | Entry size %d\n", entrySize);
  }
}

void Controller::ringCQHeadDoorbell(uint16_t qid, uint16_t head,
                                    uint64_t &tick) {
  CQueue *pQueue = ppCQueue[qid];

  pDmaEngine->write(0, 4, nullptr, tick);

  if (pQueue) {
    pQueue->setHead(head);
    /*
        DPRINTF(NVMeQueue, "CQ %-5d| Completion Queue Head Doorbell | Item count
       in queue %d | head %d | tail %d\n", qid, pQueue->getItemCount(),
       pQueue->getHead(), pQueue->getTail()); DPRINTF(NVMeBreakdown,
       "C%d|4|H%d|T%d\n", qid, pQueue->getHead(), pQueue->getTail());*/

    if (pQueue->interruptEnabled()) {
      clearInterrupt(pQueue->getInterruptVector());
    }
  }
}

void Controller::ringSQTailDoorbell(uint16_t qid, uint16_t tail,
                                    uint64_t &tick) {
  SQueue *pQueue = ppSQueue[qid];

  pDmaEngine->write(0, 4, nullptr, tick);

  if (pQueue) {
    pQueue->setTail(tail);
    /*
        DPRINTF(NVMeQueue, "SQ %-5d| Submission Queue Tail Doorbell | Item count
       in queue %d | head %d | tail %d\n", qid, pQueue->getItemCount(),
       pQueue->getHead(), pQueue->getTail()); DPRINTF(NVMeBreakdown,
       "S%d|0|H%d|T%d\n", qid, pQueue->getHead(), pQueue->getTail());*/
  }
}

void Controller::clearInterrupt(uint16_t interruptVector) {
  uint16_t notFinished = 0;

  // Check all queues associated with same interrupt vector are processed
  for (uint16_t i = 0; i < conf.readUint(NVME_MAX_IO_CQUEUE) + 1; i++) {
    if (ppCQueue[i]) {
      if (ppCQueue[i]->getInterruptVector() == interruptVector) {
        notFinished += ppCQueue[i]->getItemCount();
      }
    }
  }

  // Update interrupt
  updateInterrupt(interruptVector, notFinished > 0);
}

void Controller::updateInterrupt(uint16_t interruptVector, bool post) {
  pParent->updateInterrupt(interruptVector, post);
}

void Controller::submit(CQEntryWrapper &entry) {
  CQueue *pQueue = ppCQueue[entry.cqID];

  if (pQueue == NULL) {
    // panic("nvme_ctrl: Completion Queue not created! CQID %d\n", entry.cqid);
  }

  // Enqueue with delay
  auto iter = lCQFIFO.begin();

  for (; iter != lCQFIFO.end(); iter++) {
    if (iter->submitAt > entry.submitAt) {
      break;
    }
  }

  lCQFIFO.insert(iter, entry);
}

int Controller::createCQueue(uint16_t cqid, uint16_t size, uint16_t iv,
                             bool ien, bool pc, uint64_t prp1) {
  int ret = 1;  // Invalid Queue ID

  if (ppCQueue[cqid] == NULL) {
    ppCQueue[cqid] = new CQueue(iv, ien, cqid, size);
    ppCQueue[cqid]->setBase(new PRPList(&cfgdata, prp1, size * cqstride, pc),
                            cqstride);

    ret = 0;

    // DPRINTF(NVMeQueue, "CQ %-5d| CREATE | Entry size %d | IV %04X | IEN %s |
    // PC %s\n", cqid, entrySize, iv, BOOLEAN_STRING(ien), BOOLEAN_STRING(pc));
  }

  return ret;
}

int Controller::createSQueue(uint16_t sqid, uint16_t cqid, uint16_t size,
                             uint8_t priority, bool pc, uint64_t prp1) {
  int ret = 1;  // Invalid Queue ID

  if (ppSQueue[sqid] == NULL) {
    if (ppCQueue[cqid] != NULL) {
      ppSQueue[sqid] = new SQueue(cqid, priority, sqid, size);
      ppSQueue[sqid]->setBase(new PRPList(&cfgdata, prp1, size * sqstride, pc),
                              sqstride);

      ret = 0;

      // DPRINTF(NVMeQueue, "SQ %-5d| CREATE | Entry size %d | Priority %d | PC
      // %s\n", cqid, entrySize, priority, BOOLEAN_STRING(pc));
    }
    else {
      ret = 2;  // Invalid CQueue
    }
  }

  return ret;
}

int Controller::deleteCQueue(uint16_t cqid) {
  int ret = 0;  // Success

  if (ppCQueue[cqid] != NULL && cqid > 0) {
    for (uint16_t i = 1; i < conf.readUint(NVME_MAX_IO_CQUEUE) + 1; i++) {
      if (ppSQueue[i]) {
        if (ppSQueue[i]->getCQID() == cqid) {
          ret = 2;  // Invalid Queue Deletion
          break;
        }
      }
    }

    if (ret == 0) {
      delete ppCQueue[cqid];
      ppCQueue[cqid] = NULL;

      // DPRINTF(NVMeQueue, "CQ %-5d| DELETE\n", cqid);
    }
  }
  else {
    ret = 1;  // Invalid Queue ID
  }

  return ret;
}

int Controller::deleteSQueue(uint16_t sqid) {
  int ret = 0;  // Success

  if (ppSQueue[sqid] != NULL && sqid > 0) {
    // Create abort response
    uint16_t sqHead = ppSQueue[sqid]->getHead();
    uint16_t status = 0x8000 | (TYPE_GENERIC_COMMAND_STATUS << 9) |
                      (STATUS_ABORT_DUE_TO_SQ_DELETE << 1);

    // Abort all commands in SQueue
    for (auto iter = lSQFIFO.begin(); iter != lSQFIFO.end(); iter++) {
      if (iter->sqID == sqid) {
        CQEntryWrapper wrapper(*iter);
        wrapper.entry.dword2.sqHead = sqHead;
        wrapper.entry.dword3.status = status;
        submit(wrapper);

        iter = lSQFIFO.erase(iter);
      }
    }

    // Delete SQueue
    delete ppSQueue[sqid];
    ppSQueue[sqid] = NULL;

    // DPRINTF(NVMeQueue, "SQ %-5d| DELETE\n", sqid);
  }
  else {
    ret = 1;  // Invalid Queue ID
  }

  return ret;
}

int Controller::abort(uint16_t sqid, uint16_t cid) {
  int ret = 0;  // Not aborted
  uint16_t sqHead;
  uint16_t status;

  for (auto iter = lSQFIFO.begin(); iter != lSQFIFO.end(); iter++) {
    if (iter->sqID == sqid && iter->entry.dword0.commandID == cid) {
      CQEntry entry;

      // Create abort response
      sqHead = ppSQueue[sqid]->getHead();
      status = 0x8000 | (TYPE_GENERIC_COMMAND_STATUS << 9) |
               (STATUS_ABORT_REQUESTED << 1);

      // Submit abort
      CQEntryWrapper wrapper(*iter);
      wrapper.entry.dword2.sqHead = sqHead;
      wrapper.entry.dword3.status = status;

      submit(wrapper);

      // Remove
      iter = lSQFIFO.erase(iter);
      ret = 1;  // Aborted

      break;
    }
  }

  return ret;
}

void Controller::identify(uint8_t *data) {
  uint16_t vid, ssvid;
  uint64_t totalSize;
  uint64_t unallocated;
  uint32_t nn = pSubsystem->validNamespaceCount();

  pParent->getVendorID(vid, ssvid);
  pSubsystem->getNVMCapacity(totalSize, unallocated);

  unallocated = totalSize - unallocated;

  /** Controller Capabilities and Features **/
  {
    // PCI Vendor ID
    memcpy(data + 0x0000, &vid, 2);

    // PCI Subsystem Vendor ID
    memcpy(data + 0x0002, &ssvid, 2);

    // Serial Number
    strncpy((char *)data + 0x0004, "00000000000000000000", 0x14);

    // Model Number
    strncpy((char *)data + 0x0018, "gem5 NVMe Controller by Donghyun Gouk   ",
            0x28);

    // Firmware Revision
    strncpy((char *)data + 0x0040, "v01.0000", 0x08);

    // Recommended Arbitration Burst
    data[0x0048] = 0x00;

    // IEEE OUI Identifier (Same as Inter 750)
    {
      data[0x0049] = 0xE4;
      data[0x004A] = 0xD2;
      data[0x004B] = 0x5C;
    }

    // Controller Multi-Path I/O and Namespace Sharing Capabilities
    // [Bits ] Description
    // [07:03] Reserved
    // [02:02] 1 for SR-IOV Virtual Function, 0 for PCI (Physical) Function
    // [01:01] 1 for more than one host may connected to NVM subsystem
    // [00:00] 1 for NVM subsystem may has more than one NVM subsystem port
    data[0x004C] = 0x00;

    // Maximum Data Transfer Size
    data[0x004D] = 0x00;  // No limit

    // Controller ID
    {
      data[0x004E] = 0x00;
      data[0x004F] = 0x00;
    }

    // Version
    {
      data[0x0050] = 0x01;
      data[0x0051] = 0x02;
      data[0x0052] = 0x01;
      data[0x0053] = 0x00;
    }  // NVM Express 1.2.1 Compliant Controller

    // RTD3 Resume Latency
    {
      data[0x0054] = 0x00;
      data[0x0055] = 0x00;
      data[0x0056] = 0x00;
      data[0x0057] = 0x00;
    }  // Not reported

    // RTD3 Enter Latency
    {
      data[0x0058] = 0x00;
      data[0x0059] = 0x00;
      data[0x005A] = 0x00;
      data[0x005B] = 0x00;
    }  // Not repotred

    // Optional Asynchronous Events Supported
    {
      // [Bits ] Description
      // [31:10] Reserved
      // [09:09] 1 for Support Firmware Activation Notice
      // [08:08] 1 for Support Namespace Attributes Notice
      // [07:00] Reserved
      data[0x005C] = 0x00;
      data[0x005D] = 0x00;
      data[0x005E] = 0x00;
      data[0x005F] = 0x00;
    }

    // Controller Attributes
    {
      // [Bits ] Description
      // [31:01] Reserved
      // [00:00] 1 for Support 128-bit Host Identifier
      data[0x0060] = 0x00;
      data[0x0061] = 0x00;
      data[0x0062] = 0x00;
      data[0x0063] = 0x00;
    }
    memset(data + 0x0064, 0, 156);  // Reserved
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
      data[0x0100] = 0x08;
      data[0x0101] = 0x00;
    }

    // Abort Command Limit
    data[0x0102] = 0x03;  // Recommanded value is 4 (3 + 1)

    // Asynchronous Event Request Limit
    data[0x0103] = 0x03;  // Recommanded value is 4 (3 + 1))

    // Firmware Updates
    // [Bits ] Description
    // [07:05] Reserved
    // [04:04] 1 for Support firmware activation without a reset
    // [03:01] The number of firmware slot
    // [00:00] 1 for First firmware slot is read only, 0 for read/write
    data[0x0104] = 0x00;

    // Log Page Attributes
    // [Bits ] Description
    // [07:03] Reserved
    // [02:02] 1 for Support extended data for Get Log Page command
    // [01:01] 1 for Support Command Effects log page
    // [00:00] 1 for Support S.M.A.R.T. / Health information log page per
    //         namespace basis
    data[0x0105] = 0x01;

    // Error Log Page Entries, 0's based value
    data[0x0106] = 0x63;  // 64 entries

    // Number of Power States Support, 0's based value
    data[0x0107] = 0x00;  // 1 states

    // Admin Vendor Specific Command Configuration
    // [Bits ] Description
    // [07:01] Reserved
    // [00:00] 1 for all vendor specific commands use the format at Figure 12.
    //         0 for format is vendor specific
    data[0x0108] = 0x00;

    // Autonomous Power State Transition Attributes
    // [Bits ] Description
    // [07:01] Reserved
    // [00:00] 1 for Support autonomous power state transitions
    data[0x0109] = 0x00;

    // Warning Composite Temperature Threshold
    {
      data[0x010A] = 0x00;
      data[0x010B] = 0x00;
    }

    // Critical Composite Temperature Threshold
    {
      data[0x010C] = 0x00;
      data[0x010D] = 0x00;
    }

    // Maximum Time for Firmware Activation
    {
      data[0x010E] = 0x00;
      data[0x010F] = 0x00;
    }

    // Host Memory Buffer Preferred Size
    {
      data[0x0110] = 0x00;
      data[0x0111] = 0x00;
      data[0x0112] = 0x00;
      data[0x0113] = 0x00;
    }

    // Host Memory Buffer Minimum Size
    {
      data[0x0114] = 0x00;
      data[0x0115] = 0x00;
      data[0x0116] = 0x00;
      data[0x0117] = 0x00;
    }

    // Total NVM Capacity
    {
      memcpy(data + 0x118, &totalSize, 8);
      memset(data + 0x120, 0, 8);
    }

    // Unallocated NVM Capacity
    {
      memcpy(data + 0x118, &unallocated, 8);
      memset(data + 0x120, 0, 8);
    }

    // Replay Protected Memory Block Support
    {
      // [Bits ] Description
      // [31:24] Access Size
      // [23:16] Total Size
      // [15:06] Reserved
      // [05:03] Authentication Method
      // [02:00] Number of RPMB Units
      data[0x0138] = 0x00;
      data[0x0139] = 0x00;
      data[0x013A] = 0x00;
      data[0x013B] = 0x00;
    }

    // Reserved
    memset(data + 0x013C, 0, 4);

    // Keep Alive Support
    {
      data[0x0140] = 0x00;
      data[0x0141] = 0x00;
    }

    // Reserved
    memset(data + 0x0142, 0, 190);
  }

  /** NVM Command Set Attributes **/
  {
    // Submission Queue Entry Size
    // [Bits ] Description
    // [07:04] Maximum Submission Queue Entry Size
    // [03:00] Minimum Submission Queue Entry Size
    data[0x0200] = 0x66;  // 64Bytes, 64Bytes

    // Completion Queue Entry Size
    // [Bits ] Description
    // [07:04] Maximum Completion Queue Entry Size
    // [03:00] Minimum Completion Queue Entry Size
    data[0x0201] = 0x44;  // 16Bytes, 16Bytes

    // Maximum  Outstanding Commands
    {
      data[0x0202] = 0x00;
      data[0x0203] = 0x00;
    }

    // Number of Namespaces
    memcpy(data + 0x0204, &nn, 4);

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
      data[0x0208] = 0x04;
      data[0x0209] = 0x00;
    }

    // Fused Operation Support
    {
      // [Bits ] Description
      // [15:01] Reserved
      // [00:00] 1 for Support Compare and Write fused operation
      data[0x020A] = 0x00;
      data[0x020B] = 0x00;
    }

    // Format NVM Attributes
    // [Bits ] Description
    // [07:03] Reserved
    // [02:02] 1 for Support cryptographic erase
    // [01:01] 1 for Support cryptographic erase performed on all namespaces,
    //         0 for namespace basis
    // [00:00] 1 for Format on specific namespace results on format on all
    //         namespaces, 0 for namespace basis
    data[0x020C] = 0x00;

    // Volatile Write Cache
    // [Bits ] Description
    // [07:01] Reserved
    // [00:00] 1 for volatile write cache is present
    data[0x020D] =
        cfgdata.pConfigReader->iclConfig.readBoolean(ICL::ICL_USE_WRITE_CACHE)
            ? 0x01
            : 0x00;

    // Atomic Write Unit Normal
    {
      data[0x020E] = 0x00;
      data[0x020F] = 0x00;
    }

    // Atomic Write Unit Power Fail
    {
      data[0x0210] = 0x00;
      data[0x0211] = 0x00;
    }

    // NVM Vendor Specific Command Configuration
    // [Bits ] Description
    // [07:01] Reserved
    // [00:00] 1 for all vendor specific commands use the format at Figure 12.
    //         0 for format is vendor specific
    data[0x0212] = 0x00;

    // Reserved
    data[0x0213] = 0x00;

    // Atomic Compare & Write Unit
    {
      data[0x0214] = 0x00;
      data[0x0215] = 0x00;
    }

    // Reserved
    memset(data + 0x0216, 0, 2);

    // SGL Support
    {
      // [Bits ] Description
      // [31:21] Reserved
      // [20:20] 1 for Support Address field in SGL Data Block
      // [19:19] 1 for Support MPTR containing SGL descriptor
      // [18:18] 1 for Support MPTR/DPTR containing SGL with larger than amouont
      //         of data to be trasferred
      // [17:17] 1 for Support byte aligned contiguous physical buffer of
      //         metadata is supported
      // [16:16] 1 for Support SGL Bit Bucket descriptor
      // [15:03] Reserved
      // [02:02] 1 for Support Keyed SGL Data Block descriptor
      // [01:01] Reserved
      // [00:00] 1 for Support SGLs in NVM Command Set
      data[0x0218] = 0x00;
      data[0x0219] = 0x00;
      data[0x021A] = 0x00;
      data[0x021B] = 0x00;
    }

    // Reserved
    memset(data + 0x021C, 0, 228);

    // NVM Subsystem NVMe Qualified Name
    {
      memset(data + 0x300, 0, 0x100);
      strncpy((char *)data + 0x0300,
              "nqn.2014-08.org.nvmexpress:uuid:270a1c70-962c-4116-6f1e340b9321",
              0x44);
    }

    // Reserved
    memset(data + 0x0400, 0, 768);

    // NVMe over Fabric
    memset(data + 0x0700, 0, 256);
  }

  /** Power State Descriptors **/
  // Power State 0
  /// Descriptor
  {
    // Maximum Power
    {
      data[0x0800] = 0xC4;
      data[0x0801] = 0x09;
    }

    // Reserved
    data[0x0802] = 0x00;

    // [Bits ] Description
    // [31:26] Reserved
    // [25:25] Non-Operational State
    // [24:24] Max Power Scale
    data[0x0803] = 0x00;

    // Entry Latency
    {
      data[0x0804] = 0x00;
      data[0x0805] = 0x00;
      data[0x0806] = 0x00;
      data[0x0807] = 0x00;
    }

    // Exit Latency
    {
      data[0x0808] = 0x00;
      data[0x0809] = 0x00;
      data[0x080A] = 0x00;
      data[0x080B] = 0x00;
    }

    // [Bits   ] Description
    // [103:101] Reserved
    // [100:096] Relative Read Throughput
    data[0x080C] = 0x00;

    // [Bits   ] Description
    // [111:109] Reserved
    // [108:104] Relative Read Latency
    data[0x080D] = 0x00;

    // [Bits   ] Description
    // [119:117] Reserved
    // [116:112] Relative Write Throughput
    data[0x080E] = 0x00;

    // [Bits   ] Description
    // [127:125] Reserved
    // [124:120] Relative Write Latency
    data[0x080E] = 0x00;

    // Idle Power
    {
      data[0x080F] = 0x00;
      data[0x0810] = 0x00;
    }

    // [Bits   ] Description
    // [151:150] Idle Power Scale
    // [149:144] Reserved
    data[0x0811] = 0x00;

    // Reserved
    data[0x0812] = 0x00;

    // Active Power
    {
      data[0x0813] = 0x00;
      data[0x0814] = 0x00;
    }

    // [Bits   ] Description
    // [183:182] Active Power Scale
    // [181:179] Reserved
    // [178:176] Active Power Workload
    data[0x0815] = 0x00;

    // Reserved
    memset(data + 0x0816, 0, 9);
  }

  // PSD1 ~ PSD31
  memset(data + 0x0820, 0, 992);

  // Vendor specific area
  memset(data + 0x0C00, 0, 1024);
}

void Controller::collectSQueue(uint64_t &tick) {
  static uint16_t sqcount = conf.readUint(NVME_MAX_IO_SQUEUE) + 1;
  static uint16_t wrrHigh = conf.readUint(NVME_WRR_HIGH);
  static uint16_t wrrMedium = conf.readUint(NVME_WRR_MEDIUM);

  // Check ready
  if (!(registers.status & 0x00000001)) {
    return;
  }

  // Round robin
  if (arbitration == ROUND_ROBIN) {
    SQueue *pQueue;

    uint16_t updated = 0;

    while (true) {
      for (uint16_t i = 0; i < sqcount; i++) {
        pQueue = ppSQueue[i];

        if (pQueue) {
          if (checkQueue(pQueue, lSQFIFO, tick)) {
            updated++;
          }
        }
      }

      if (updated == 0) {
        break;
      }

      updated = 0;
    }
  }
  // Weighted round robin
  else if (arbitration == WEIGHTED_ROUND_ROBIN) {
    SQueue *pQueue;

    uint16_t updated = 0;

    // Collect all Admin Commands
    pQueue = ppSQueue[0];

    while (true) {
      if (!checkQueue(pQueue, lSQFIFO, tick)) {
        break;
      }
    }

    // Round robin all urgent command queues
    while (true) {
      for (uint16_t i = 1; i < sqcount; i++) {
        pQueue = ppSQueue[i];

        if (pQueue) {
          if (pQueue->getPriority() == PRIORITY_URGENT) {
            if (checkQueue(pQueue, lSQFIFO, tick)) {
              updated++;
            }
          }
        }
      }

      if (updated == 0) {
        break;
      }

      updated = 0;
    }

    // Weighted Round robin
    uint32_t total_updated = 0;

    while (true) {
      // Round robin all high-priority command queues
      for (uint16_t i = 1; i < sqcount; i++) {
        pQueue = ppSQueue[i];

        if (pQueue) {
          if (pQueue->getPriority() == PRIORITY_HIGH) {
            if (checkQueue(pQueue, lSQFIFO, tick)) {
              updated++;
              total_updated++;

              if (updated == wrrHigh) {
                updated = 0;
                break;
              }
            }
          }
        }
      }

      // Round robin all medium-priority command queues
      for (uint16_t i = 1; i < sqcount; i++) {
        pQueue = ppSQueue[i];

        if (pQueue) {
          if (pQueue->getPriority() == PRIORITY_MEDIUM) {
            if (checkQueue(pQueue, lSQFIFO, tick)) {
              updated++;
              total_updated++;

              if (updated == wrrMedium) {
                updated = 0;
                break;
              }
            }
          }
        }
      }

      // Round robin all low-priority command queues
      for (uint16_t i = 1; i < sqcount; i++) {
        pQueue = ppSQueue[i];

        if (pQueue) {
          if (pQueue->getPriority() == PRIORITY_MEDIUM) {
            if (checkQueue(pQueue, lSQFIFO, tick)) {
              total_updated++;

              break;
            }
          }
        }
      }

      // Check finished
      if (total_updated == 0) {
        break;
      }

      total_updated = 0;
    }
  }
  else {
    // panic("nvme_ctrl: Invalid arbitration method\n");
  }
}

void Controller::work(uint64_t &tick) {
  // Check ready
  if (!(registers.status & 0x00000001)) {
    return;
  }

  // Check CQFIFO
  if (lCQFIFO.size() > 0) {
    CQueue *pQueue;

    for (auto iter = lCQFIFO.begin(); iter != lCQFIFO.end(); iter++) {
      if (iter->submitAt <= tick) {
        pQueue = ppCQueue[iter->cqID];

        // Write CQ
        tick = pQueue->setData(&iter->entry, tick);

        // Delete entry
        iter = lCQFIFO.erase(iter);

        // Collect interrupt vector
        if (pQueue->interruptEnabled()) {
          // Update interrupt
          updateInterrupt(pQueue->getInterruptVector(), true);
        }
      }
    }
  }

  // Check SQFIFO
  if (lSQFIFO.size() > 0) {
    SQEntryWrapper front = lSQFIFO.front();
    CQEntryWrapper response(front);
    lSQFIFO.pop_front();

    // Process command
    if (pSubsystem->submitCommand(front, response, tick)) {
      submit(response);
    }
  }
}

bool Controller::checkQueue(SQueue *pQueue, std::list<SQEntryWrapper> &fifo,
                            uint64_t &tick) {
  SQEntry entry;

  if (pQueue->getItemCount() > 0) {
    tick = pQueue->getData(&entry, tick);
    fifo.push_back(SQEntryWrapper(entry, pQueue->getID(), pQueue->getHead(),
                                  pQueue->getCQID()));

    return true;
  }

  return false;
}

}  // namespace NVMe

}  // namespace HIL

}  // namespace SimpleSSD
