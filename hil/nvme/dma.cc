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

#include "hil/nvme/dma.hh"

#include "log/trace.hh"
#include "util/algorithm.hh"

namespace SimpleSSD {

namespace HIL {

namespace NVMe {

DMAInterface::DMAInterface(ConfigData *cfg) : pInterface(cfg->pInterface) {}

DMAInterface::~DMAInterface() {}

PRP::PRP() : addr(0), size(0) {}

PRP::PRP(uint64_t address, uint64_t s) : addr(address), size(s) {}

PRPList::PRPList(ConfigData *cfg, uint64_t prp1, uint64_t prp2, uint64_t size)
    : DMAInterface(cfg), totalSize(size), pagesize(cfg->memoryPageSize) {
  uint64_t prp1Size = getPRPSize(prp1);
  uint64_t prp2Size = getPRPSize(prp2);

  // Determine PRP1 and PRP2
  if (totalSize <= pagesize) {
    if (totalSize <= prp1Size) {
      // PRP1 is PRP pointer, PRP2 is not used
      prpList.push_back(PRP(prp1, totalSize));
    }
    else {
      // PRP1 is PRP pointer, PRP2 is PRP pointer
      prpList.push_back(PRP(prp1, prp1Size));
      prpList.push_back(PRP(prp2, prp2Size));

      if (prp1Size + prp2Size < totalSize) {
        // TODO: panic("prp_list: Invalid DPTR size\n");
      }
    }
  }
  else if (totalSize <= pagesize * 2) {
    if (prp1Size == pagesize) {
      // PRP1 is PRP pointer, PRP2 is PRP pointer
      prpList.push_back(PRP(prp1, prp1Size));
      prpList.push_back(PRP(prp2, prp2Size));

      if (prp1Size + prp2Size < totalSize) {
        // TODO: panic("prp_list: Invalid DPTR size\n");
      }
    }
    else {
      // PRP1 is PRP pointer, PRP2 is PRP list
      prpList.push_back(PRP(prp1, prp1Size));
      getPRPListFromPRP(prp2, totalSize - prp1Size);
    }
  }
  else {
    // PRP1 is PRP pointer, PRP2 is PRP list
    prpList.push_back(PRP(prp1, prp1Size));
    getPRPListFromPRP(prp2, totalSize - prp1Size);
  }
}

PRPList::PRPList(ConfigData *cfg, uint64_t base, uint64_t size, bool cont)
    : DMAInterface(cfg), totalSize(size), pagesize(cfg->memoryPageSize) {
  if (cont) {
    prpList.push_back(PRP(base, size));
  }
  else {
    getPRPListFromPRP(base, size);
  }
}

PRPList::~PRPList() {}

void PRPList::getPRPListFromPRP(uint64_t base, uint64_t size) {
  uint64_t currentSize = 0;
  uint8_t *buffer = nullptr;

  // Get PRP size
  uint64_t prpSize = getPRPSize(base);

  // Allocate buffer
  buffer = (uint8_t *)malloc(prpSize);

  if (buffer) {
    uint64_t listPRP;
    uint64_t listPRPSize;
    uint64_t tick = 0;

    // Read PRP
    pInterface->dmaRead(base, prpSize, buffer, tick);

    for (size_t i = 0; i < prpSize; i += 8) {
      listPRP = *((uint64_t *)(buffer + i));
      listPRPSize = getPRPSize(listPRP);
      currentSize += listPRPSize;

      if (listPRP == 0) {
        // TODO: panic("prp_list: Invalid PRP in PRP List\n");
      }

      prpList.push_back(PRP(listPRP, listPRPSize));

      if (currentSize >= size) {
        break;
      }
    }

    free(buffer);

    if (currentSize < size) {
      // PRP list ends but size is not full
      // Last item of PRP list is pointer of another PRP list
      listPRP = prpList.back().addr;
      prpList.pop_back();

      getPRPListFromPRP(listPRP, size - currentSize);
    }
  }
  else {
    // TODO: panic("prp_list: ENOMEM\n");
  }
}

uint64_t PRPList::getPRPSize(uint64_t addr) {
  return pagesize - (addr & (pagesize - 1));
}

uint64_t PRPList::read(uint64_t offset, uint64_t length, uint8_t *buffer,
                       uint64_t &tick) {
  uint64_t delay = 0;
  uint64_t totalRead = 0;
  uint64_t currentOffset = 0;
  uint64_t read;
  bool begin = false;

  for (auto &iter : prpList) {
    if (begin) {
      read = MIN(iter.size, length - totalRead);
      pInterface->dmaRead(iter.addr, read, buffer ? buffer + totalRead : NULL,
                          tick);
      totalRead += read;

      if (totalRead == length) {
        break;
      }
    }

    if (!begin && currentOffset + iter.size > offset) {
      begin = true;
      totalRead = offset - currentOffset;
      read = MIN(iter.size - totalRead, length);
      delay = pInterface->dmaRead(iter.addr + totalRead, read, buffer, tick);
      totalRead = read;
    }

    currentOffset += iter.size;
  }

  // TODO: DPRINTF(NVMeDMA, "DMAPORT | READ  | Tick %" PRIu64 "\n",
  // totalDMATime);

  return delay;
}

uint64_t PRPList::write(uint64_t offset, uint64_t length, uint8_t *buffer,
                        uint64_t &tick) {
  uint64_t delay = 0;
  uint64_t totalWritten = 0;
  uint64_t currentOffset = 0;
  uint64_t written;
  bool begin = false;

  for (auto &iter : prpList) {
    if (begin) {
      written = MIN(iter.size, length - totalWritten);
      pInterface->dmaWrite(iter.addr, written,
                           buffer ? buffer + totalWritten : NULL, tick);
      totalWritten += written;

      if (totalWritten == length) {
        break;
      }
    }

    if (!begin && currentOffset + iter.size > offset) {
      begin = true;
      totalWritten = offset - currentOffset;
      written = MIN(iter.size - totalWritten, length);
      delay =
          pInterface->dmaWrite(iter.addr + totalWritten, written, buffer, tick);
      totalWritten = written;
    }

    currentOffset += iter.size;
  }

  // TODO: DPRINTF(NVMeDMA, "DMAPORT | WRITE | Tick %" PRIu64 "\n",
  // totalDMATime);

  return delay;
}

SGLDescriptor::SGLDescriptor() {
  memset(data, 0, 16);
}

Chunk::Chunk() : addr(0), length(0), ignore(true) {}

Chunk::Chunk(uint64_t a, uint32_t l, bool i) : addr(a), length(l), ignore(i) {}

SGL::SGL(ConfigData *cfg, uint64_t prp1, uint64_t prp2)
    : DMAInterface(cfg), totalSize(0) {
  SGLDescriptor desc;

  // Create first SGL descriptor from PRP pointers
  memcpy(desc.data, &prp1, 8);
  memcpy(desc.data + 8, &prp2, 8);

  // Check type
  if (SGL_TYPE(desc.id) == TYPE_DATA_BLOCK_DESCRIPTOR ||
      SGL_TYPE(desc.id) == TYPE_KEYED_DATA_BLOCK_DESCRIPTOR) {
    // This is entire buffer
    parseSGLDescriptor(desc);
  }
  else if (SGL_TYPE(desc.id) == TYPE_SEGMENT_DESCRIPTOR ||
           SGL_TYPE(desc.id) == TYPE_LAST_SEGMENT_DESCRIPTOR) {
    // This points segment
    parseSGLSegment(desc.address, desc.length);
  }
}

SGL::~SGL() {}

void SGL::parseSGLDescriptor(SGLDescriptor &desc) {
  switch (SGL_TYPE(desc.id)) {
    case TYPE_DATA_BLOCK_DESCRIPTOR:
    case TYPE_KEYED_DATA_BLOCK_DESCRIPTOR:
      list.push_back(Chunk(desc.address, desc.length, false));
      totalSize += desc.length;

      break;
    case TYPE_BIT_BUCKET_DESCRIPTOR:
      list.push_back(Chunk(desc.address, desc.length, true));
      totalSize += desc.length;

      break;
    default:
      Logger::panic("Invalid SGL descriptor");
      break;
  }

  if (SGL_SUBTYPE(desc.id) != SUBTYPE_ADDRESS) {
    Logger::panic("Unexpected SGL subtype");
  }
}

void SGL::parseSGLSegment(uint64_t address, uint32_t length) {
  uint8_t *buffer = nullptr;
  uint64_t tick = 0;
  bool next = false;

  // Allocate buffer
  buffer = (uint8_t *)calloc(length, 1);

  // Read segment
  pInterface->dmaRead(address, length, buffer, tick);

  // Parse SGL descriptor
  SGLDescriptor desc;

  for (uint32_t i = 0; i < length; i += 16) {
    memcpy(desc.data, buffer + i, 16);

    switch (SGL_TYPE(desc.id)) {
      case TYPE_DATA_BLOCK_DESCRIPTOR:
      case TYPE_KEYED_DATA_BLOCK_DESCRIPTOR:
      case TYPE_BIT_BUCKET_DESCRIPTOR:
        parseSGLDescriptor(desc);

        break;
      case TYPE_SEGMENT_DESCRIPTOR:
      case TYPE_LAST_SEGMENT_DESCRIPTOR:
        if (i != length - 16) {
          Logger::panic("Invalid SGL segment");
        }
        next = true;

        break;
    }
  }

  // Go to next
  // TODO: fix potential stack overflow
  if (next) {
    parseSGLSegment(desc.address, desc.length);
  }
}

uint64_t SGL::read(uint64_t offset, uint64_t length, uint8_t *buffer,
                   uint64_t &tick) {
  uint64_t delay = 0;
  uint64_t totalRead = 0;
  uint64_t currentOffset = 0;
  uint64_t read;
  bool begin = false;

  for (auto &iter : list) {
    if (begin) {
      read = MIN(iter.length, length - totalRead);

      if (!iter.ignore) {
        pInterface->dmaRead(iter.addr, read, buffer ? buffer + totalRead : NULL,
                            tick);
      }

      totalRead += read;

      if (totalRead == length) {
        break;
      }
    }

    if (!begin && currentOffset + iter.length > offset) {
      begin = true;
      totalRead = offset - currentOffset;
      read = MIN(iter.length - totalRead, length);

      if (!iter.ignore) {
        delay = pInterface->dmaRead(iter.addr + totalRead, read, buffer, tick);
      }

      totalRead = read;
    }

    currentOffset += iter.length;
  }

  // TODO: DPRINTF(NVMeDMA, "DMAPORT | READ  | Tick %" PRIu64 "\n",
  // totalDMATime);

  return delay;
}

uint64_t SGL::write(uint64_t offset, uint64_t length, uint8_t *buffer,
                    uint64_t &tick) {
  uint64_t delay = 0;
  uint64_t totalWritten = 0;
  uint64_t currentOffset = 0;
  uint64_t written;
  bool begin = false;

  for (auto &iter : list) {
    if (begin) {
      written = MIN(iter.length, length - totalWritten);

      if (!iter.ignore) {
        pInterface->dmaWrite(iter.addr, written,
                             buffer ? buffer + totalWritten : NULL, tick);
      }

      totalWritten += written;

      if (totalWritten == length) {
        break;
      }
    }

    if (!begin && currentOffset + iter.length > offset) {
      begin = true;
      totalWritten = offset - currentOffset;
      written = MIN(iter.length - totalWritten, length);

      if (!iter.ignore) {
        delay = pInterface->dmaWrite(iter.addr + totalWritten, written, buffer,
                                     tick);
      }

      totalWritten = written;
    }

    currentOffset += iter.length;
  }

  // TODO: DPRINTF(NVMeDMA, "DMAPORT | WRITE | Tick %" PRIu64 "\n",
  // totalDMATime);

  return delay;
}

}  // namespace NVMe

}  // namespace HIL

}  // namespace SimpleSSD
