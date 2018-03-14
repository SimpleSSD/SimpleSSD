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

#include "hil/nvme/namespace.hh"

#include "hil/nvme/subsystem.hh"
#include "log/trace.hh"
#include "util/algorithm.hh"

namespace SimpleSSD {

namespace HIL {

namespace NVMe {

Namespace::Namespace(Subsystem *p, ConfigData *c)
    : pParent(p),
      pDisk(nullptr),
      pCfgdata(c),
      conf(c->pConfigReader->nvmeConfig),
      nsid(NSID_NONE),
      attached(false),
      allocated(false),
      formatFinishedAt(0) {}

Namespace::~Namespace() {
  if (pDisk) {
    delete pDisk;
  }
}

bool Namespace::submitCommand(SQEntryWrapper &req, CQEntryWrapper &resp,
                              uint64_t &tick) {
  uint64_t beginAt = tick;

  if (beginAt < formatFinishedAt) {
    resp.makeStatus(false, false, TYPE_GENERIC_COMMAND_STATUS,
                    STATUS_FORMAT_IN_PROGRESS);

    return true;
  }

  // Admin commands
  if (req.sqID == 0) {
    switch (req.entry.dword0.opcode) {
      case OPCODE_GET_LOG_PAGE:
        getLogPage(req, resp, beginAt);
        break;
      default:
        resp.makeStatus(true, false, TYPE_GENERIC_COMMAND_STATUS,
                        STATUS_INVALID_OPCODE);
        break;
    }
  }

  // NVM commands
  else {
    switch (req.entry.dword0.opcode) {
      case OPCODE_FLUSH:
        flush(req, resp, beginAt);
        break;
      case OPCODE_WRITE:
        write(req, resp, beginAt);
        break;
      case OPCODE_READ:
        read(req, resp, beginAt);
        break;
      case OPCODE_DATASET_MANAGEMEMT:
        datasetManagement(req, resp, beginAt);
        break;
      default:
        resp.makeStatus(true, false, TYPE_GENERIC_COMMAND_STATUS,
                        STATUS_INVALID_OPCODE);
        break;
    }
  }

  resp.submitAt = beginAt;

  return true;
}

void Namespace::setData(uint32_t id, Information *data) {
  nsid = id;
  memcpy(&info, data, sizeof(Information));

  if (conf.readBoolean(NVME_ENABLE_DISK_IMAGE) && id == NSID_LOWEST) {
    uint64_t diskSize;

    if (conf.readBoolean(NVME_USE_COW_DISK)) {
      pDisk = new CoWDisk();
    }
    else {
      pDisk = new Disk();
    }

    std::string filename = conf.readString(NVME_DISK_IMAGE_PATH);

    diskSize = pDisk->open(filename, info.size * info.lbaSize, info.lbaSize);

    if (diskSize == 0) {
      Logger::panic("Failed to open disk image");
    }
    else if (diskSize != info.size * info.lbaSize) {
      if (conf.readBoolean(NVME_STRICT_DISK_SIZE)) {
        Logger::panic("Disk size not match");
      }
    }
  }

  allocated = true;
}

void Namespace::attach(bool attach) {
  attached = attach;
}

uint32_t Namespace::getNSID() {
  return nsid;
}

Namespace::Information *Namespace::getInfo() {
  return &info;
}

bool Namespace::isAttached() {
  return attached;
}

void Namespace::format(uint64_t tick) {
  formatFinishedAt = tick;

  health = HealthInfo();

  if (pDisk) {
    delete pDisk;
    pDisk = nullptr;
  }
}

void Namespace::getLogPage(SQEntryWrapper &req, CQEntryWrapper &resp,
                           uint64_t &tick) {
  uint16_t numdl = (req.entry.dword10 & 0xFFFF0000) >> 16;
  uint16_t lid = req.entry.dword10 & 0xFFFF;
  uint16_t numdu = req.entry.dword11 & 0xFFFF;
  uint32_t lopl = req.entry.dword12;
  uint32_t lopu = req.entry.dword13;

  uint32_t req_size = (((uint32_t)numdu << 16 | numdl) + 1) * 4;
  uint64_t offset = ((uint64_t)lopu << 32) | lopl;

  DMAInterface *dma = nullptr;

  Logger::debugprint(Logger::LOG_HIL_NVME,
                     "ADMIN   | Get Log Page | Log %d | Size %d | NSID %d", lid,
                     req_size, nsid);

  if (req.useSGL) {
    dma = new SGL(pCfgdata, req.entry.data1, req.entry.data2);
  }
  else {
    dma = new PRPList(pCfgdata, req.entry.data1, req.entry.data2,
                      (uint64_t)req_size);
  }

  switch (lid) {
    case LOG_SMART_HEALTH_INFORMATION:
      if (req.entry.namespaceID == nsid) {
        dma->write(offset, 512, health.data, tick);
      }
      else {
        resp.makeStatus(true, false, TYPE_COMMAND_SPECIFIC_STATUS,
                        STATUS_NAMESPACE_NOT_ATTACHED);
      }

      break;
    default:
      resp.makeStatus(true, false, TYPE_COMMAND_SPECIFIC_STATUS,
                      STATUS_INVALID_LOG_PAGE);
      break;
  }

  delete dma;
}

void Namespace::flush(SQEntryWrapper &req, CQEntryWrapper &resp,
                      uint64_t &tick) {
  bool err = false;

  if (!attached) {
    err = true;
    resp.makeStatus(true, false, TYPE_COMMAND_SPECIFIC_STATUS,
                    STATUS_NAMESPACE_NOT_ATTACHED);
  }

  Logger::debugprint(Logger::LOG_HIL_NVME, "NVM     | FLUSH | NSID %-5d", nsid);

  if (!err) {
    uint64_t beginAt = tick;
    pParent->flush(this, tick);

    Logger::debugprint(Logger::LOG_HIL_NVME,
                       "NVM     | FLUSH | NSID %-5d| %" PRIu64 " - %" PRIu64
                       " (%" PRIu64 ")",
                       nsid, beginAt, tick, tick - beginAt);
  }
}

void Namespace::write(SQEntryWrapper &req, CQEntryWrapper &resp,
                      uint64_t &tick) {
  bool err = false;

  uint64_t slba = ((uint64_t)req.entry.dword11 << 32) | req.entry.dword10;
  uint16_t nlb = (req.entry.dword12 & 0xFFFF) + 1;

  DMAInterface *dma = nullptr;

  if (!attached) {
    err = true;
    resp.makeStatus(true, false, TYPE_COMMAND_SPECIFIC_STATUS,
                    STATUS_NAMESPACE_NOT_ATTACHED);
  }
  if (nlb == 0) {
    err = true;
    Logger::warn("nvme_namespace: host tried to write 0 blocks");
  }

  Logger::debugprint(Logger::LOG_HIL_NVME, "NVM     | WRITE | NSID %-5d", nsid);

  if (!err) {
    uint64_t dmaTick = tick;
    uint64_t dmaDelayed;

    if (req.useSGL) {
      dma = new SGL(pCfgdata, req.entry.data1, req.entry.data2);
    }
    else {
      dma = new PRPList(pCfgdata, req.entry.data1, req.entry.data2,
                        (uint64_t)nlb * info.lbaSize);
    }

    if (pDisk) {
      uint8_t *buffer = (uint8_t *)calloc(nlb, info.lbaSize);

      dmaDelayed = dma->read(0, nlb * info.lbaSize, buffer, dmaTick);
      pDisk->write(slba, nlb, buffer);

      free(buffer);
    }
    else {
      dmaDelayed = dma->read(0, nlb * info.lbaSize, nullptr, dmaTick);
    }

    delete dma;

    Logger::debugprint(Logger::LOG_HIL_NVME,
                       "NVM     | WRITE | %" PRIX64 " + %d | DMA %" PRIu64
                       " - %" PRIu64 " (%" PRIu64 ")",
                       slba, nlb, dmaDelayed, dmaTick, dmaTick - dmaDelayed);

    tick = dmaTick;

    pParent->write(this, slba, nlb, tick);

    Logger::debugprint(Logger::LOG_HIL_NVME,
                       "NVM     | WRITE | %" PRIX64 " + %d | NAND %" PRIu64
                       " - %" PRIu64 " (%" PRIu64 ")",
                       slba, nlb, dmaTick, tick, tick - dmaTick);
  }
}

void Namespace::read(SQEntryWrapper &req, CQEntryWrapper &resp,
                     uint64_t &tick) {
  bool err = false;

  uint64_t slba = ((uint64_t)req.entry.dword11 << 32) | req.entry.dword10;
  uint16_t nlb = (req.entry.dword12 & 0xFFFF) + 1;
  // bool fua = req.entry.dword12 & 0x40000000;

  DMAInterface *dma = nullptr;

  if (!attached) {
    err = true;
    resp.makeStatus(true, false, TYPE_COMMAND_SPECIFIC_STATUS,
                    STATUS_NAMESPACE_NOT_ATTACHED);
  }
  if (nlb == 0) {
    err = true;
    Logger::warn("nvme_namespace: host tried to read 0 blocks");
  }

  Logger::debugprint(Logger::LOG_HIL_NVME, "NVM     | READ  | NSID %-5d", nsid);

  if (!err) {
    uint64_t dmaTick = tick;
    uint64_t dmaDelayed;

    if (req.useSGL) {
      dma = new SGL(pCfgdata, req.entry.data1, req.entry.data2);
    }
    else {
      dma = new PRPList(pCfgdata, req.entry.data1, req.entry.data2,
                        (uint64_t)nlb * info.lbaSize);
    }

    pParent->read(this, slba, nlb, tick);

    Logger::debugprint(Logger::LOG_HIL_NVME,
                       "NVM     | READ  | %" PRIX64 " + %d | NAND %" PRIu64
                       " - %" PRIu64 " (%" PRIu64 ")",
                       slba, nlb, dmaTick, tick, tick - dmaTick);

    dmaTick = tick;

    if (pDisk) {
      uint8_t *buffer = (uint8_t *)calloc(nlb, info.lbaSize);

      pDisk->read(slba, nlb, buffer);
      dmaDelayed = dma->write(0, nlb * info.lbaSize, buffer, dmaTick);

      free(buffer);
    }
    else {
      dmaDelayed = dma->write(0, nlb * info.lbaSize, nullptr, dmaTick);
    }

    delete dma;

    Logger::debugprint(Logger::LOG_HIL_NVME,
                       "NVM     | READ  | %" PRIX64 " + %d | DMA %" PRIu64
                       " - %" PRIu64 " (%" PRIu64 ")",
                       slba, nlb, dmaDelayed, dmaTick, dmaTick - dmaDelayed);

    tick = MAX(tick, dmaTick);
  }
}

void Namespace::datasetManagement(SQEntryWrapper &req, CQEntryWrapper &resp,
                                  uint64_t &tick) {
  bool err = false;

  int nr = (req.entry.dword10 & 0xFF) + 1;
  bool ad = req.entry.dword11 & 0x04;

  DMAInterface *dma = nullptr;

  if (!attached) {
    err = true;
    resp.makeStatus(true, false, TYPE_COMMAND_SPECIFIC_STATUS,
                    STATUS_NAMESPACE_NOT_ATTACHED);
  }
  if (!ad) {
    err = true;
    // Just ignore
  }

  Logger::debugprint(Logger::LOG_HIL_NVME,
                     "NVM     | TRIM  | NSID %-5d| %d ranges | Attr %1X", nsid,
                     nr, req.entry.dword11 & 0x0F);

  if (!err) {
    static DatasetManagementRange range;
    uint64_t beginAt = tick;

    if (req.useSGL) {
      dma = new SGL(pCfgdata, req.entry.data1, req.entry.data2);
    }
    else {
      dma = new PRPList(pCfgdata, req.entry.data1, req.entry.data2,
                        (uint64_t)nr * 0x10);
    }

    for (int i = 0; i < nr; i++) {
      dma->read(i * 0x10, 0x10, range.data, tick);
      pParent->trim(this, range.slba, range.nlb, tick);
    }

    delete dma;

    Logger::debugprint(Logger::LOG_HIL_NVME,
                       "NVM     | TRIM  | NSID %-5d| %" PRIu64 " - %" PRIu64
                       " (%" PRIu64 ")",
                       nsid, beginAt, tick, tick - beginAt);
  }
}

}  // namespace NVMe

}  // namespace HIL

}  // namespace SimpleSSD
