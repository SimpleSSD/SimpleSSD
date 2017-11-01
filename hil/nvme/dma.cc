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
#include "util/algorithm.hh"

namespace SimpleSSD {

namespace HIL {

namespace NVMe {

DMAScheduler::DMAScheduler(Interface *intr, Config *conf)
    : interface(intr), lastReadEndAt(0), lastWriteEndAt(0) {
  psPerByte = conf->readFloat(NVME_DMA_DELAY);
}

uint64_t DMAScheduler::read(uint64_t addr, uint64_t length, uint8_t *buffer,
                            uint64_t &tick) {
  uint64_t latency = (uint64_t)(psPerByte * length + 0.5);
  uint64_t delay = 0;

  // DMA Scheduling
  if (tick == 0) {
    tick = lastReadEndAt;
  }

  if (lastReadEndAt <= tick) {
    lastReadEndAt = tick + latency;
  }
  else {
    delay = lastReadEndAt - tick;
    lastReadEndAt += latency;
  }

  // Read data
  // DPRINTF(NVMeDMA, "DMAPORT | READ  | %016" PRIX64 " + %" PRIX64 "\n",
  // dmaAddr + offset, size);

  if (buffer) {
    interface->dmaRead(addr, length, buffer);

    // // Print data
    // if (size <= 64) {
    //   DPRINTF(NVMeDMA, "DMAPORT | READ  | ----------------\n");
    //   for (int i = 0; i < size / 8; i++) {
    //     DPRINTF(NVMeDMA, "DMAPORT | READ  | %016" PRIX64 "\n", *((uint64_t
    //     *)buffer + i));
    //   }
    //   DPRINTF(NVMeDMA, "DMAPORT | READ  | ----------------\n");
    // }
  }

  delay += tick;
  tick = delay + latency;

  return delay;
}

uint64_t DMAScheduler::write(uint64_t addr, uint64_t length, uint8_t *buffer,
                             uint64_t &tick) {
  uint64_t latency = (uint64_t)(psPerByte * length + 0.5);
  uint64_t delay = 0;

  // DMA Scheduling
  if (tick == 0) {
    tick = lastWriteEndAt;
  }

  if (lastWriteEndAt <= tick) {
    lastWriteEndAt = tick + latency;
  }
  else {
    delay = lastWriteEndAt - tick;
    lastWriteEndAt += latency;
  }

  // Read data
  // DPRINTF(NVMeDMA, "DMAPORT | READ  | %016" PRIX64 " + %" PRIX64 "\n",
  // dmaAddr + offset, size);

  if (buffer) {
    interface->dmaWrite(addr, length, buffer);

    // // Print data
    // if (size <= 64) {
    //   DPRINTF(NVMeDMA, "DMAPORT | READ  | ----------------\n");
    //   for (int i = 0; i < size / 8; i++) {
    //     DPRINTF(NVMeDMA, "DMAPORT | READ  | %016" PRIX64 "\n", *((uint64_t
    //     *)buffer + i));
    //   }
    //   DPRINTF(NVMeDMA, "DMAPORT | READ  | ----------------\n");
    // }
  }

  delay += tick;
  tick = delay + latency;

  return delay;
}

PRP::PRP() : addr(0), size(0) {}

PRP::PRP(uint64_t address, uint64_t s) : addr(address), size(s) {}

PRPList::PRPList(ConfigData *cfg, uint64_t prp1, uint64_t prp2, uint64_t size)
    : dmaEngine(cfg->pDmaEngine),
      totalSize(size),
      pagesize(cfg->memoryPageSize) {
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
    : dmaEngine(cfg->pDmaEngine),
      totalSize(size),
      pagesize(cfg->memoryPageSize) {
  if (cont) {
    prpList.push_back(PRP(base, size));
  }
  else {
    getPRPListFromPRP(base, size);
  }
}

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
    dmaEngine->read(base, prpSize, buffer, tick);

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
      dmaEngine->read(iter.addr, read, buffer ? buffer + totalRead : NULL,
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
      delay = dmaEngine->read(iter.addr, read, buffer, tick);
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
      dmaEngine->write(iter.addr, written,
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
      delay = dmaEngine->write(iter.addr, written, buffer, tick);
      totalWritten = written;
    }

    currentOffset += iter.size;
  }

  // TODO: DPRINTF(NVMeDMA, "DMAPORT | WRITE | Tick %" PRIu64 "\n",
  // totalDMATime);

  return delay;
}

}  // namespace NVMe

}  // namespace HIL

}  // namespace SimpleSSD
