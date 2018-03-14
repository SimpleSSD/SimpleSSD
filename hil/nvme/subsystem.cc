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

#include "hil/nvme/subsystem.hh"

#include <algorithm>
#include <cmath>

#include "hil/nvme/controller.hh"
#include "log/trace.hh"
#include "util/algorithm.hh"

namespace SimpleSSD {

namespace HIL {

namespace NVMe {

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

Subsystem::Subsystem(Controller *ctrl, ConfigData *cfg)
    : pParent(ctrl),
      pCfgdata(cfg),
      conf(cfg->pConfigReader->nvmeConfig),
      allocatedLogicalPages(0),
      commandCount(0) {
  pHIL = new HIL(cfg->pConfigReader);

  pHIL->getLPNInfo(totalLogicalPages, logicalPageSize);

  if (conf.readBoolean(NVME_ENABLE_DEFAULT_NAMESPACE)) {
    Namespace::Information info;
    uint32_t lba = (uint32_t)conf.readUint(NVME_LBA_SIZE);

    for (info.lbaFormatIndex = 0; info.lbaFormatIndex < nLBAFormat;
         info.lbaFormatIndex++) {
      if (lbaSize[info.lbaFormatIndex] == lba) {
        info.lbaSize = lbaSize[info.lbaFormatIndex];

        break;
      }
    }

    if (info.lbaFormatIndex == nLBAFormat) {
      Logger::panic("Failed to setting LBA size (512B ~ 4KB)");
    }

    // Fill Namespace information
    info.size = totalLogicalPages * logicalPageSize / lba;
    info.capacity = info.size;
    info.dataProtectionSettings = 0x00;
    info.namespaceSharingCapabilities = 0x00;

    if (createNamespace(NSID_LOWEST, &info)) {
      lNamespaces.front()->attach(true);
    }
    else {
      Logger::panic("Failed to create namespace");
    }
  }
}

Subsystem::~Subsystem() {
  for (auto &iter : lNamespaces) {
    delete iter;
  }

  delete pHIL;
}

bool Subsystem::createNamespace(uint32_t nsid, Namespace::Information *info) {
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
  for (auto &iter : lNamespaces) {
    allocated.push_back(iter->getInfo()->range);
  }

  // Sort
  allocated.sort([](const LPNRange &a, const LPNRange &b) -> bool {
    return a.slpn < b.slpn;
  });

  // Merge
  auto iter = allocated.begin();
  auto next = iter;

  while (true) {
    next++;

    if (iter != allocated.end() || next == allocated.end()) {
      break;
    }

    if (iter->slpn + iter->nlp == next->slpn) {
      iter = allocated.erase(iter);
      next = iter;
    }
    else {
      iter++;
    }
  }

  // Invert
  unallocated.push_back(LPNRange(0, totalLogicalPages));

  for (auto &iter : allocated) {
    // Split last item
    auto &last = unallocated.back();

    if (last.slpn <= iter.slpn &&
        last.slpn + last.nlp >= iter.slpn + iter.nlp) {
      unallocated.push_back(LPNRange(
          iter.slpn + iter.nlp, last.slpn + last.nlp - iter.slpn - iter.nlp));
      last.nlp = iter.slpn - last.slpn;
    }
    else {
      Logger::panic("BUG");
    }
  }

  // Allocated unallocated area to namespace
  for (auto iter = unallocated.begin(); iter != unallocated.end(); iter++) {
    if (iter->nlp >= requestedLogicalPages) {
      info->range = *iter;
      info->range.nlp = requestedLogicalPages;

      break;
    }
  }

  if (info->range.nlp == 0) {
    return false;
  }

  allocatedLogicalPages += requestedLogicalPages;

  // Fill Information
  info->sizeInByteL = requestedLogicalPages * logicalPageSize;
  info->sizeInByteH = 0;

  // Create namespace
  Namespace *pNS = new Namespace(this, pCfgdata);
  pNS->setData(nsid, info);

  lNamespaces.push_back(pNS);
  Logger::debugprint(Logger::LOG_HIL_NVME,
                     "NS %-5d| CREATE | LBA size %" PRIu32
                     " | Capacity %" PRIu64,
                     nsid, info->lbaSize, info->size);

  return true;
}

bool Subsystem::destroyNamespace(uint32_t nsid) {
  bool found = false;
  Namespace::Information *info;

  for (auto iter = lNamespaces.begin(); iter != lNamespaces.end(); iter++) {
    if ((*iter)->getNSID() == nsid) {
      found = true;

      Logger::debugprint(Logger::LOG_HIL_NVME, "NS %-5d| DELETE", nsid);

      info = (*iter)->getInfo();
      allocatedLogicalPages -= info->size * info->lbaSize / logicalPageSize;

      delete *iter;

      lNamespaces.erase(iter);

      break;
    }
  }

  return found;
}

void Subsystem::fillIdentifyNamespace(uint8_t *buffer,
                                      Namespace::Information *info) {
  // Namespace Size
  memcpy(buffer + 0, &info->size, 8);

  // Namespace Capacity
  memcpy(buffer + 8, &info->capacity, 8);

  // Namespace Utilization
  info->utilization =
      pHIL->getUsedPageCount() * logicalPageSize / info->lbaSize;
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

  // LBA Formats
  for (uint32_t i = 0; i < nLBAFormat; i++) {
    memcpy(buffer + 128 + i * 4, lbaFormat + i, 4);
  }
}

bool Subsystem::submitCommand(SQEntryWrapper &req, CQEntryWrapper &resp,
                              uint64_t &tick) {
  bool processed = false;
  bool submit = true;
  uint64_t beginAt = tick;

  // TODO: change this
  tick += 1000000;  // 1us of command delay

  commandCount++;

  // Admin command
  if (req.sqID == 0) {
    switch (req.entry.dword0.opcode) {
      case OPCODE_DELETE_IO_SQUEUE:
        processed = deleteSQueue(req, resp, beginAt);
        break;
      case OPCODE_CREATE_IO_SQUEUE:
        processed = createSQueue(req, resp, beginAt);
        break;
      case OPCODE_GET_LOG_PAGE:
        processed = getLogPage(req, resp, beginAt);
        break;
      case OPCODE_DELETE_IO_CQUEUE:
        processed = deleteCQueue(req, resp, beginAt);
        break;
      case OPCODE_CREATE_IO_CQUEUE:
        processed = createCQueue(req, resp, beginAt);
        break;
      case OPCODE_IDENTIFY:
        processed = identify(req, resp, beginAt);
        break;
      case OPCODE_ABORT:
        processed = abort(req, resp, beginAt);
        break;
      case OPCODE_SET_FEATURES:
        processed = setFeatures(req, resp, beginAt);
        break;
      case OPCODE_GET_FEATURES:
        processed = getFeatures(req, resp, beginAt);
        break;
      case OPCODE_ASYNC_EVENT_REQ:
        submit = false;
        break;
      case OPCODE_NAMESPACE_MANAGEMENT:
        processed = namespaceManagement(req, resp, beginAt);
        break;
      case OPCODE_NAMESPACE_ATTACHMENT:
        processed = namespaceAttachment(req, resp, beginAt);
        break;
      case OPCODE_FORMAT_NVM:
        processed = formatNVM(req, resp, beginAt);
        break;
      default:
        resp.makeStatus(true, false, TYPE_GENERIC_COMMAND_STATUS,
                        STATUS_INVALID_OPCODE);
        break;
    }
  }

  // NVM commands or Namespace specific Admin commands
  if (!processed) {
    if (req.entry.namespaceID < NSID_ALL) {
      for (auto &iter : lNamespaces) {
        if (iter->getNSID() == req.entry.namespaceID) {
          return iter->submitCommand(req, resp, beginAt);
        }
      }
    }
    else {
      for (auto &iter : lNamespaces) {
        iter->submitCommand(req, resp, beginAt);
      }

      return true;
    }
  }

  // Invalid namespace
  if (!processed) {
    resp.makeStatus(false, false, TYPE_GENERIC_COMMAND_STATUS,
                    STATUS_ABORT_INVALID_NAMESPACE);
  }

  resp.submitAt = beginAt;

  return submit;
}

void Subsystem::getNVMCapacity(uint64_t &total, uint64_t &used) {
  total = totalLogicalPages * logicalPageSize;
  used = allocatedLogicalPages * logicalPageSize;
}

uint32_t Subsystem::validNamespaceCount() {
  return (uint32_t)lNamespaces.size();
}

void Subsystem::read(Namespace *ns, uint64_t slba, uint64_t nlblk,
                     uint64_t &tick) {
  ICL::Request req;
  Namespace::Information *info = ns->getInfo();
  uint32_t lbaratio = logicalPageSize / info->lbaSize;
  uint64_t slpn;
  uint64_t nlp;
  uint64_t off;

  slpn = slba / lbaratio;
  off = slba % lbaratio;
  nlp = (nlblk + off + lbaratio - 1) / lbaratio;

  req.range.slpn = slpn + info->range.slpn;
  req.range.nlp = nlp;
  req.offset = off * info->lbaSize;
  req.length = nlblk * info->lbaSize;

  pHIL->read(req, tick);
}

void Subsystem::write(Namespace *ns, uint64_t slba, uint64_t nlblk,
                      uint64_t &tick) {
  ICL::Request req;
  Namespace::Information *info = ns->getInfo();
  uint32_t lbaratio = logicalPageSize / info->lbaSize;
  uint64_t slpn;
  uint64_t nlp;
  uint64_t off;

  slpn = slba / lbaratio;
  off = slba % lbaratio;
  nlp = (nlblk + off + lbaratio - 1) / lbaratio;

  req.range.slpn = slpn + info->range.slpn;
  req.range.nlp = nlp;
  req.offset = off * info->lbaSize;
  req.length = nlblk * info->lbaSize;

  pHIL->write(req, tick);
}

void Subsystem::flush(Namespace *ns, uint64_t &tick) {
  ICL::Request req;
  Namespace::Information *info = ns->getInfo();

  req.range.slpn = info->range.slpn;
  req.range.nlp = info->range.nlp;
  req.offset = 0;
  req.length = info->range.nlp * logicalPageSize;

  pHIL->flush(req, tick);
}

void Subsystem::trim(Namespace *ns, uint64_t slba, uint64_t nlblk,
                     uint64_t &tick) {
  ICL::Request req;
  Namespace::Information *info = ns->getInfo();
  uint32_t lbaratio = logicalPageSize / info->lbaSize;
  uint64_t slpn;
  uint64_t nlp;
  uint64_t off;

  slpn = slba / lbaratio;
  off = slba % lbaratio;
  nlp = (nlblk + off + lbaratio - 1) / lbaratio;

  req.range.slpn = slpn + info->range.slpn;
  req.range.nlp = nlp;
  req.offset = off * info->lbaSize;
  req.length = nlblk * info->lbaSize;

  pHIL->trim(req, tick);
}

bool Subsystem::deleteSQueue(SQEntryWrapper &req, CQEntryWrapper &resp,
                             uint64_t &tick) {
  uint16_t sqid = req.entry.dword10 & 0xFFFF;

  Logger::debugprint(Logger::LOG_HIL_NVME,
                     "ADMIN   | Delete I/O Submission Queue");

  int ret = pParent->deleteSQueue(sqid);

  if (ret == 1) {
    resp.makeStatus(false, false, TYPE_COMMAND_SPECIFIC_STATUS,
                    STATUS_INVALID_QUEUE_ID);
  }

  return true;
}

bool Subsystem::createSQueue(SQEntryWrapper &req, CQEntryWrapper &resp,
                             uint64_t &tick) {
  bool err = false;

  uint16_t sqid = req.entry.dword10 & 0xFFFF;
  uint16_t entrySize = ((req.entry.dword10 & 0xFFFF0000) >> 16) + 1;
  uint16_t cqid = (req.entry.dword11 & 0xFFFF0000) >> 16;
  uint8_t priority = (req.entry.dword11 & 0x06) >> 1;
  bool pc = req.entry.dword11 & 0x01;

  Logger::debugprint(Logger::LOG_HIL_NVME,
                     "ADMIN   | Create I/O Submission Queue");

  if (entrySize > pCfgdata->maxQueueEntry) {
    err = true;
    resp.makeStatus(false, false, TYPE_COMMAND_SPECIFIC_STATUS,
                    STATUS_INVALID_QUEUE_SIZE);
  }

  if (!err) {
    int ret = pParent->createSQueue(sqid, cqid, entrySize, priority, pc,
                                    req.entry.data1);

    if (ret == 1) {
      resp.makeStatus(false, false, TYPE_COMMAND_SPECIFIC_STATUS,
                      STATUS_INVALID_QUEUE_ID);
    }
    else if (ret == 2) {
      resp.makeStatus(false, false, TYPE_COMMAND_SPECIFIC_STATUS,
                      STATUS_INVALID_COMPLETION_QUEUE);
    }
  }

  return true;
}

bool Subsystem::getLogPage(SQEntryWrapper &req, CQEntryWrapper &resp,
                           uint64_t &tick) {
  uint16_t numdl = (req.entry.dword10 & 0xFFFF0000) >> 16;
  uint16_t lid = req.entry.dword10 & 0xFFFF;
  uint16_t numdu = req.entry.dword11 & 0xFFFF;
  uint32_t lopl = req.entry.dword12;
  uint32_t lopu = req.entry.dword13;
  bool ret = false;

  uint32_t req_size = (((uint32_t)numdu << 16 | numdl) + 1) * 4;
  uint64_t offset = ((uint64_t)lopu << 32) | lopl;

  DMAInterface *dma = nullptr;

  Logger::debugprint(Logger::LOG_HIL_NVME,
                     "ADMIN   | Get Log Page | Log %d | Size %d | NSID %d", lid,
                     req_size, req.entry.namespaceID);

  if (req.useSGL) {
    dma = new SGL(pCfgdata, req.entry.data1, req.entry.data2);
  }
  else {
    dma = new PRPList(pCfgdata, req.entry.data1, req.entry.data2,
                      (uint64_t)req_size);
  }

  switch (lid) {
    case LOG_ERROR_INFORMATION:
      ret = true;
      break;
    case LOG_SMART_HEALTH_INFORMATION:
      if (req.entry.namespaceID == NSID_ALL) {
        ret = true;
        dma->write(offset, 512, globalHealth.data, tick);
      }
      break;
    case LOG_FIRMWARE_SLOT_INFORMATION:
      ret = true;
      break;
    default:
      ret = true;
      resp.makeStatus(true, false, TYPE_COMMAND_SPECIFIC_STATUS,
                      STATUS_INVALID_LOG_PAGE);
      break;
  }

  delete dma;

  return ret;
}

bool Subsystem::deleteCQueue(SQEntryWrapper &req, CQEntryWrapper &resp,
                             uint64_t &tick) {
  uint16_t cqid = req.entry.dword10 & 0xFFFF;

  Logger::debugprint(Logger::LOG_HIL_NVME,
                     "ADMIN   | Delete I/O Completion Queue");

  int ret = pParent->deleteCQueue(cqid);

  if (ret == 1) {
    resp.makeStatus(false, false, TYPE_COMMAND_SPECIFIC_STATUS,
                    STATUS_INVALID_QUEUE_ID);
  }
  else if (ret == 2) {
    resp.makeStatus(false, false, TYPE_COMMAND_SPECIFIC_STATUS,
                    STATUS_INVALID_QUEUE_DELETE);
  }

  return true;
}

bool Subsystem::createCQueue(SQEntryWrapper &req, CQEntryWrapper &resp,
                             uint64_t &tick) {
  bool err = false;

  uint16_t cqid = req.entry.dword10 & 0xFFFF;
  uint16_t entrySize = ((req.entry.dword10 & 0xFFFF0000) >> 16) + 1;
  uint16_t iv = (req.entry.dword11 & 0xFFFF0000) >> 16;
  bool ien = req.entry.dword11 & 0x02;
  bool pc = req.entry.dword11 & 0x01;

  Logger::debugprint(Logger::LOG_HIL_NVME,
                     "ADMIN   | Create I/O Completion Queue");

  if (entrySize > pCfgdata->maxQueueEntry) {
    err = true;
    resp.makeStatus(false, false, TYPE_COMMAND_SPECIFIC_STATUS,
                    STATUS_INVALID_QUEUE_SIZE);
  }

  if (!err) {
    int ret =
        pParent->createCQueue(cqid, entrySize, iv, ien, pc, req.entry.data1);

    if (ret == 1) {
      resp.makeStatus(false, false, TYPE_COMMAND_SPECIFIC_STATUS,
                      STATUS_INVALID_QUEUE_ID);
    }
  }

  return true;
}

bool Subsystem::identify(SQEntryWrapper &req, CQEntryWrapper &resp,
                         uint64_t &tick) {
  bool err = false;

  uint8_t cns = req.entry.dword10 & 0xFF;
  uint16_t cntid = (req.entry.dword10 & 0xFFFF0000) >> 16;
  static uint8_t data[0x1000];
  bool ret = true;

  uint16_t idx = 0;
  DMAInterface *dma = nullptr;

  memset(data, 0, 0x1000);
  Logger::debugprint(Logger::LOG_HIL_NVME,
                     "ADMIN   | Identify | CNS %d | CNTID %d | NSID %d", cns,
                     cntid, req.entry.namespaceID);

  if (req.useSGL) {
    dma = new SGL(pCfgdata, req.entry.data1, req.entry.data2);
  }
  else {
    dma = new PRPList(pCfgdata, req.entry.data1, req.entry.data2,
                      (uint64_t)0x1000);
  }

  switch (cns) {
    case CNS_IDENTIFY_NAMESPACE:
      if (req.entry.namespaceID == NSID_ALL) {
        // FIXME: Not called by current(4.9.32) NVMe driver
      }
      else {
        for (auto &iter : lNamespaces) {
          if (iter->isAttached() && iter->getNSID() == req.entry.namespaceID) {
            fillIdentifyNamespace(data, iter->getInfo());
          }
        }
      }

      break;
    case CNS_IDENTIFY_CONTROLLER:
      pParent->identify(data);

      break;
    case CNS_ACTIVE_NAMESPACE_LIST:
      if (req.entry.namespaceID >= NSID_ALL - 1) {
        err = true;
        resp.makeStatus(true, false, TYPE_GENERIC_COMMAND_STATUS,
                        STATUS_ABORT_INVALID_NAMESPACE);
      }
      else {
        for (auto &iter : lNamespaces) {
          if (iter->isAttached() && iter->getNSID() > req.entry.namespaceID) {
            ((uint32_t *)data)[idx++] = iter->getNSID();
          }
        }
      }

      break;
    case CNS_ALLOCATED_NAMESPACE_LIST:
      if (req.entry.namespaceID >= NSID_ALL - 1) {
        err = true;
        resp.makeStatus(true, false, TYPE_GENERIC_COMMAND_STATUS,
                        STATUS_ABORT_INVALID_NAMESPACE);
      }
      else {
        for (auto &iter : lNamespaces) {
          if (iter->getNSID() > req.entry.namespaceID) {
            ((uint32_t *)data)[idx++] = iter->getNSID();
          }
        }
      }

      break;
    case CNS_IDENTIFY_ALLOCATED_NAMESPACE:
      for (auto &iter : lNamespaces) {
        if (iter->getNSID() == req.entry.namespaceID) {
          fillIdentifyNamespace(data, iter->getInfo());
        }
      }

      break;
    case CNS_ATTACHED_CONTROLLER_LIST:
      // Only one controller
      if (cntid == 0) {
        ((uint16_t *)data)[idx++] = 1;
      }

      break;
    case CNS_CONTROLLER_LIST:
      // Only one controller
      if (cntid == 0) {
        ((uint16_t *)data)[idx++] = 1;
      }

      break;
  }

  if (ret && !err) {
    dma->write(0, 0x1000, data, tick);
  }

  delete dma;

  return ret;
}

bool Subsystem::abort(SQEntryWrapper &req, CQEntryWrapper &resp,
                      uint64_t &tick) {
  uint16_t sqid = req.entry.dword10 & 0xFFFF;
  uint16_t cid = (req.entry.dword10 & 0xFFFF0000) >> 16;

  Logger::debugprint(Logger::LOG_HIL_NVME, "ADMIN   | Abort | SQID %d | CID %d",
                     sqid, cid);

  int ret = pParent->abort(sqid, cid);

  resp.makeStatus(true, false, TYPE_GENERIC_COMMAND_STATUS, STATUS_SUCCESS);

  if (ret == 0) {
    resp.entry.dword0 = 1;
  }
  else {
    resp.entry.dword0 = 0;
  }

  return true;
}

bool Subsystem::setFeatures(SQEntryWrapper &req, CQEntryWrapper &resp,
                            uint64_t &tick) {
  bool err = false;

  uint16_t fid = req.entry.dword10 & 0x00FF;
  bool save = req.entry.dword10 & 0x80000000;

  Logger::debugprint(Logger::LOG_HIL_NVME,
                     "ADMIN   | Set Features | Feature %d | NSID %d", fid,
                     req.entry.namespaceID);

  if (save) {
    err = true;
    resp.makeStatus(false, false, TYPE_COMMAND_SPECIFIC_STATUS,
                    STATUS_FEATURE_ID_NOT_SAVEABLE);
  }

  if (!err) {
    switch (fid) {
      case FEATURE_NUMBER_OF_QUEUES:
        if ((req.entry.dword11 & 0xFFFF) == 0xFFFF ||
            (req.entry.dword11 & 0xFFFF0000) == 0xFFFF0000) {
          resp.makeStatus(false, false, TYPE_GENERIC_COMMAND_STATUS,
                          STATUS_INVALID_FIELD);
        }
        else {
          if ((req.entry.dword11 & 0xFFFF) >=
              conf.readUint(NVME_MAX_IO_SQUEUE)) {
            req.entry.dword11 = (req.entry.dword11 & 0xFFFF0000) |
                                (conf.readUint(NVME_MAX_IO_SQUEUE) - 1);
          }
          if (((req.entry.dword11 & 0xFFFF0000) >> 16) >=
              conf.readUint(NVME_MAX_IO_CQUEUE)) {
            req.entry.dword11 =
                (req.entry.dword11 & 0xFFFF) |
                ((uint32_t)(conf.readUint(NVME_MAX_IO_CQUEUE) - 1) << 16);
          }

          resp.entry.dword0 = req.entry.dword11;
          queueAllocated = resp.entry.dword0;
        }

        break;
      case FEATURE_INTERRUPT_COALESCING:
        pParent->setCoalescingParameter((req.entry.dword11 >> 8) & 0xFF,
                                        req.entry.dword11 & 0xFF);
        break;
      case FEATURE_INTERRUPT_VECTOR_CONFIGURATION:
        pParent->setCoalescing(req.entry.dword11 & 0xFFFF,
                               req.entry.dword11 & 0x10000);
        break;
      default:
        resp.makeStatus(true, false, TYPE_GENERIC_COMMAND_STATUS,
                        STATUS_INVALID_FIELD);
        break;
    }
  }

  return true;
}

bool Subsystem::getFeatures(SQEntryWrapper &req, CQEntryWrapper &resp,
                            uint64_t &tick) {
  uint16_t fid = req.entry.dword10 & 0x00FF;

  Logger::debugprint(Logger::LOG_HIL_NVME,
                     "ADMIN   | Get Features | Feature %d | NSID %d", fid,
                     req.entry.namespaceID);

  switch (fid) {
    case FEATURE_ARBITRATION:
      resp.entry.dword0 = ((uint8_t)(conf.readUint(NVME_WRR_HIGH) *
                                         conf.readUint(NVME_WRR_MEDIUM) -
                                     1)
                           << 24) |
                          ((conf.readUint(NVME_WRR_MEDIUM) - 1) << 16) | 0x03;
      break;
    case FEATURE_VOLATILE_WRITE_CACHE:
      resp.entry.dword0 = pCfgdata->pConfigReader->iclConfig.readBoolean(
                              ICL::ICL_USE_WRITE_CACHE)
                              ? 0x01
                              : 0x00;
      break;
    case FEATURE_NUMBER_OF_QUEUES:
      resp.entry.dword0 = queueAllocated;
      break;
    case FEATURE_INTERRUPT_COALESCING:
      pParent->getCoalescingParameter(resp.entry.data + 1, resp.entry.data);
      break;
    case FEATURE_INTERRUPT_VECTOR_CONFIGURATION:
      resp.entry.dword0 = (req.entry.dword11 & 0xFFFF) | 0;

      if (pParent->getCoalescing(req.entry.dword11 & 0xFFFF)) {
        resp.entry.dword0 |= 0x10000;
      }

      break;
    default:
      resp.makeStatus(true, false, TYPE_GENERIC_COMMAND_STATUS,
                      STATUS_INVALID_FIELD);
      break;
  }

  return true;
}

bool Subsystem::asyncEventReq(SQEntryWrapper &req, CQEntryWrapper &resp,
                              uint64_t &tick) {
  // FIXME: Not implemented
  return true;
}

bool Subsystem::namespaceManagement(SQEntryWrapper &req, CQEntryWrapper &resp,
                                    uint64_t &tick) {
  bool err = false;

  uint8_t sel = req.entry.dword10 & 0x0F;

  DMAInterface *dma = nullptr;

  Logger::debugprint(Logger::LOG_HIL_NVME,
                     "ADMIN   | Namespace Management | OP %d | NSID %d", sel,
                     req.entry.namespaceID);

  if ((sel == 0x00 && req.entry.namespaceID != NSID_NONE) || sel > 0x01) {
    err = true;
    resp.makeStatus(false, false, TYPE_GENERIC_COMMAND_STATUS,
                    STATUS_INVALID_FIELD);
  }

  if (!err) {
    if (sel == 0x00) {  // Create
      uint32_t nsid = NSID_LOWEST;

      for (auto &iter : lNamespaces) {
        if (iter->getNSID() == nsid) {
          nsid++;
        }
        else {
          break;
        }
      }

      if (nsid == NSID_ALL) {
        resp.makeStatus(false, false, TYPE_COMMAND_SPECIFIC_STATUS,
                        STATUS_NAMESPACE_ID_UNAVAILABLE);
      }
      else {
        static uint8_t data[0x1000];
        static Namespace::Information info;

        if (req.useSGL) {
          dma = new SGL(pCfgdata, req.entry.data1, req.entry.data2);
        }
        else {
          dma = new PRPList(pCfgdata, req.entry.data1, req.entry.data2,
                            (uint64_t)0x1000);
        }

        dma->read(0, 0x1000, data, tick);

        delete dma;

        // Copy data
        memcpy(&info.size, data + 0, 8);
        memcpy(&info.capacity, data + 8, 8);
        info.lbaFormatIndex = data[26];
        info.dataProtectionSettings = data[29];
        info.namespaceSharingCapabilities = data[30];

        if (info.lbaFormatIndex >= nLBAFormat) {
          resp.makeStatus(false, false, TYPE_GENERIC_COMMAND_STATUS,
                          STATUS_ABORT_INVALID_FORMAT);
        }
        else if (info.namespaceSharingCapabilities != 0x00) {
          resp.makeStatus(false, false, TYPE_GENERIC_COMMAND_STATUS,
                          STATUS_INVALID_FIELD);
        }
        else if (info.dataProtectionSettings != 0x00) {
          resp.makeStatus(false, false, TYPE_GENERIC_COMMAND_STATUS,
                          STATUS_INVALID_FIELD);
        }

        info.lbaSize = lbaSize[info.lbaFormatIndex];

        bool ret = createNamespace(nsid, &info);

        if (ret) {
          resp.entry.dword0 = nsid;
        }
        else {
          resp.makeStatus(false, false, TYPE_COMMAND_SPECIFIC_STATUS,
                          STATUS_NAMESPACE_INSUFFICIENT_CAPACITY);
        }
      }
    }
    else {  // Delete
      bool found = destroyNamespace(req.entry.namespaceID);

      if (!found) {
        resp.makeStatus(false, false, TYPE_GENERIC_COMMAND_STATUS,
                        STATUS_ABORT_INVALID_NAMESPACE);
      }
    }
  }

  return true;
}

bool Subsystem::namespaceAttachment(SQEntryWrapper &req, CQEntryWrapper &resp,
                                    uint64_t &tick) {
  bool err = false;

  uint8_t sel = req.entry.dword10 & 0x0F;

  uint32_t ctrlList;
  DMAInterface *dma = nullptr;

  Logger::debugprint(Logger::LOG_HIL_NVME,
                     "ADMIN   | Namespace Attachment | OP %d | NSID %d", sel,
                     req.entry.namespaceID);

  if (req.useSGL) {
    dma = new SGL(pCfgdata, req.entry.data1, req.entry.data2);
  }
  else {
    dma = new PRPList(pCfgdata, req.entry.data1, req.entry.data2,
                      (uint64_t)0x1000);
  }

  dma->read(0, 4, (uint8_t *)&ctrlList, tick);

  delete dma;

  if (ctrlList != 0x00000001) {
    err = true;
    resp.makeStatus(false, false, TYPE_COMMAND_SPECIFIC_STATUS,
                    STATUS_CONTROLLER_LIST_INVALID);
  }

  if (!err) {
    if (sel == 0x00) {  // Attach
      for (auto &iter : lNamespaces) {
        if (iter->getNSID() == req.entry.namespaceID) {
          if (iter->isAttached()) {
            resp.makeStatus(true, false, TYPE_COMMAND_SPECIFIC_STATUS,
                            STATUS_NAMESPACE_ALREADY_ATTACHED);
          }
          else {
            iter->attach(true);
          }
        }
      }
    }
    else if (sel == 0x01) {  // Detach
      for (auto &iter : lNamespaces) {
        if (iter->getNSID() == req.entry.namespaceID) {
          if (iter->isAttached()) {
            iter->attach(false);
          }
          else {
            resp.makeStatus(true, false, TYPE_COMMAND_SPECIFIC_STATUS,
                            STATUS_NAMESPACE_NOT_ATTACHED);
          }
        }
      }
    }
    else {
      resp.makeStatus(false, false, TYPE_GENERIC_COMMAND_STATUS,
                      STATUS_INVALID_FIELD);
    }
  }

  return true;
}

bool Subsystem::formatNVM(SQEntryWrapper &req, CQEntryWrapper &resp,
                          uint64_t &tick) {
  bool err = false;

  uint8_t ses = (req.entry.dword10 & 0x0E00) >> 9;
  bool pil = req.entry.dword10 & 0x0100;
  uint8_t pi = (req.entry.dword10 & 0xE0) >> 5;
  bool mset = req.entry.dword10 & 0x10;
  uint8_t lbaf = req.entry.dword10 & 0x0F;

  Logger::debugprint(Logger::LOG_HIL_NVME,
                     "ADMIN   | Format NVM | SES %d | NSID %d", ses,
                     req.entry.namespaceID);

  if (ses != 0x00 && ses != 0x01) {
    err = true;
  }
  if (pil || mset) {
    err = true;
  }
  if (pi != 0x00) {
    err = true;
  }
  if (lbaf >= nLBAFormat) {
    err = true;
  }

  if (err) {
    resp.makeStatus(false, false, TYPE_GENERIC_COMMAND_STATUS,
                    STATUS_INVALID_FORMAT);
  }
  else {
    // Update Namespace
    auto iter = lNamespaces.begin();
    Namespace::Information *info = nullptr;

    for (; iter != lNamespaces.end(); iter++) {
      if ((*iter)->getNSID() == req.entry.namespaceID) {
        info = (*iter)->getInfo();

        break;
      }
    }

    if (info) {
      info->lbaFormatIndex = lbaf;
      info->lbaSize = lbaSize[lbaf];
      info->size = totalLogicalPages * logicalPageSize / info->lbaSize;
      info->capacity = info->size;

      // Send format command to HIL
      pHIL->format(info->range, ses == 0x01, tick);

      // Reset health stat and set format progress
      (*iter)->format(tick);

      info->utilization =
          pHIL->getUsedPageCount() * logicalPageSize / info->lbaSize;
    }
    else {
      resp.makeStatus(false, false, TYPE_GENERIC_COMMAND_STATUS,
                      STATUS_ABORT_INVALID_NAMESPACE);
    }
  }

  return true;
}

void Subsystem::getStats(std::vector<Stats> &list) {
  Stats temp;

  temp.name = "command_count";
  temp.desc = "Total number of NVMe command handled";
  list.push_back(temp);

  pHIL->getStats(list);
}

void Subsystem::getStatValues(std::vector<uint64_t> &values) {
  values.push_back(commandCount);

  pHIL->getStatValues(values);
}

void Subsystem::resetStats() {
  commandCount = 0;

  pHIL->resetStats();
}

}  // namespace NVMe

}  // namespace HIL

}  // namespace SimpleSSD
