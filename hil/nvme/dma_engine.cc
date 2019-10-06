// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "hil/nvme/dma_engine.hh"

#define MAKE_SGL_ID(type, subtype)                                             \
  (uint8_t)(((type << 4) & 0xF0) | (subtype & 0x0F))
#define SGL_TYPE(id) (uint8_t)(id >> 4)
#define SGL_SUBTYPE(id) (uint8_t)(id & 0x0F)

enum SGL_DESCRIPTOR_TYPE : uint8_t {
  TYPE_DATA_BLOCK_DESCRIPTOR = 0x00,
  TYPE_BIT_BUCKET_DESCRIPTOR = 0x01,
  TYPE_SEGMENT_DESCRIPTOR = 0x02,
  TYPE_LAST_SEGMENT_DESCRIPTOR = 0x03,
  TYPE_KEYED_DATA_BLOCK_DESCRIPTOR = 0x04
};

enum SGL_DESCRIPTOR_SUBTYPE : uint8_t {
  SUBTYPE_ADDRESS = 0x00,
  SUBTYPE_OFFSET = 0x01,
  SUBTYPE_NVME_TRANSPORT_SPECIFIC = 0x02
};

namespace SimpleSSD::HIL::NVMe {

PRPEngine::PRP::PRP() : address(0), size(0) {}

PRPEngine::PRP::PRP(uint64_t a, uint64_t s) : address(a), size(s) {}

PRPEngine::PRPInitContext::PRPInitContext()
    : handledSize(0), bufferSize(0), buffer(nullptr) {}

PRPEngine::PRPEngine(ObjectData &o, DMAInterface *i, uint64_t p)
    : DMAEngine(o, i), inited(false), totalSize(0), pageSize(p) {
  panic_if(popcount64(p) != 1, "Invalid memory page size provided.");

  readPRPList =
      createEvent([this](uint64_t t) { getPRPListFromPRP_readDone(t); },
                  "HIL::NVMe::PRPEngine::readPRPList");
}

PRPEngine::~PRPEngine() {}

void PRPEngine::getPRPListFromPRP_readDone(uint64_t now) {
  uint64_t listPRP;
  uint64_t listPRPSize;

  for (size_t i = 0; i < prpContext.bufferSize; i += 8) {
    listPRP = *((uint64_t *)(prpContext.buffer + i));
    listPRPSize = getSizeFromPRP(listPRP);

    if (listPRP == 0) {
      panic("prp_list: Invalid PRP in PRP List");
    }

    prpContext.handledSize += listPRPSize;
    prpList.push_back(PRP(listPRP, listPRPSize));

    if (prpContext.handledSize >= totalSize) {
      break;
    }
  }

  free(prpContext.buffer);

  if (prpContext.handledSize < totalSize) {
    // PRP list ends but size is not full
    // Last item of PRP list is pointer of another PRP list
    listPRP = prpList.back().address;
    listPRPSize = getSizeFromPRP(listPRP);

    prpList.pop_back();

    prpContext.bufferSize = listPRPSize;
    prpContext.buffer = (uint8_t *)malloc(prpContext.bufferSize);

    panic_if(prpContext.buffer == nullptr, "Out of memory.");

    pInterface->read(listPRP, prpContext.bufferSize, prpContext.buffer,
                     readPRPList);
  }
  else {
    // Done
    inited = true;

    schedule(dmaContext.eid, now);
  }
}

uint64_t PRPEngine::getSizeFromPRP(uint64_t prp) {
  return pageSize - (prp & (pageSize - 1));
}

void PRPEngine::getPRPListFromPRP(uint64_t prp, Event eid) {
  dmaContext.eid = eid;
  prpContext.handledSize = 0;
  prpContext.bufferSize = getSizeFromPRP(prp);
  prpContext.buffer = (uint8_t *)malloc(prpContext.bufferSize);

  panic_if(prpContext.buffer == nullptr, "Out of memory.");

  pInterface->read(prp, prpContext.bufferSize, prpContext.buffer, readPRPList);
}

void PRPEngine::init(uint64_t prp1, uint64_t prp2, uint64_t sizeLimit,
                     Event eid) {
  bool immediate = true;
  uint8_t mode = 0xFF;
  uint64_t prp1Size = getSizeFromPRP(prp1);
  uint64_t prp2Size = getSizeFromPRP(prp2);

  totalSize = sizeLimit;

  // Determine PRP1 and PRP2
  if (totalSize <= pageSize) {
    if (totalSize <= prp1Size) {
      mode = 0;
    }
    else {
      mode = 1;
    }
  }
  else if (totalSize <= pageSize * 2) {
    if (prp1Size == pageSize) {
      mode = 1;
    }
    else {
      mode = 2;
    }
  }
  else {
    mode = 2;
  }

  switch (mode) {
    case 0:
      // PRP1 is PRP pointer, PRP2 is not used
      prpList.push_back(PRP(prp1, totalSize));

      break;
    case 1:
      // PRP1 and PRP2 are PRP pointer
      prpList.push_back(PRP(prp1, prp1Size));
      prpList.push_back(PRP(prp2, prp2Size));

      panic_if(prp1Size + prp2Size < totalSize, "Invalid DPTR size");

      break;
    case 2:
      immediate = false;

      // PRP1 is PRP pointer, PRP2 is PRP list
      prpList.push_back(PRP(prp1, prp1Size));
      getPRPListFromPRP(prp2, eid);

      break;
    default:
      panic("Invalid PRP1/2 while parsing.");

      break;
  }

  if (immediate) {
    inited = true;

    schedule(eid, getTick());
  }
}

void PRPEngine::initQueue(uint64_t base, uint64_t size, bool cont, Event eid) {
  totalSize = size;

  if (cont) {
    prpList.push_back(PRP(base, size));

    inited = true;
    schedule(eid, getTick());
  }
  else {
    getPRPListFromPRP(base, eid);
  }
}

void PRPEngine::read(uint64_t offset, uint64_t length, uint8_t *buffer,
                     Event eid) {
  panic_if(!inited, "Accessed to uninitialized PRPEngine.");

  uint64_t totalRead = 0;
  uint64_t currentOffset = 0;
  uint64_t read;
  bool begin = false;

  dmaContext.eid = eid;
  dmaContext.counter = 0;

  for (auto &iter : prpList) {
    if (begin) {
      read = MIN(iter.size, length - totalRead);
      dmaContext.counter++;
      pInterface->read(iter.address, read, buffer ? buffer + totalRead : NULL,
                       dmaHandler);
      totalRead += read;

      if (totalRead == length) {
        break;
      }
    }

    if (!begin && currentOffset + iter.size > offset) {
      begin = true;
      totalRead = offset - currentOffset;
      read = MIN(iter.size - totalRead, length);
      dmaContext.counter++;
      pInterface->read(iter.address + totalRead, read, buffer, dmaHandler);
      totalRead = read;
    }

    currentOffset += iter.size;
  }

  if (dmaContext.counter == 0) {
    dmaContext.counter = 1;

    dmaDone(getTick());
  }
}

void PRPEngine::write(uint64_t offset, uint64_t length, uint8_t *buffer,
                      Event eid) {
  panic_if(!inited, "Accessed to uninitialized PRPEngine.");

  uint64_t totalWritten = 0;
  uint64_t currentOffset = 0;
  uint64_t written;
  bool begin = false;

  dmaContext.eid = eid;
  dmaContext.counter = 0;

  for (auto &iter : prpList) {
    if (begin) {
      written = MIN(iter.size, length - totalWritten);
      dmaContext.counter++;
      pInterface->write(iter.address, written,
                        buffer ? buffer + totalWritten : NULL, dmaHandler);
      totalWritten += written;

      if (totalWritten == length) {
        break;
      }
    }

    if (!begin && currentOffset + iter.size > offset) {
      begin = true;
      totalWritten = offset - currentOffset;
      written = MIN(iter.size - totalWritten, length);
      dmaContext.counter++;
      pInterface->write(iter.address + totalWritten, written, buffer,
                        dmaHandler);
      totalWritten = written;
    }

    currentOffset += iter.size;
  }

  if (dmaContext.counter == 0) {
    dmaContext.counter = 1;

    dmaDone(getTick());
  }
}

void PRPEngine::createCheckpoint(std::ostream &out) noexcept {
  DMAEngine::createCheckpoint(out);

  BACKUP_SCALAR(out, inited);
  BACKUP_SCALAR(out, totalSize);
  BACKUP_SCALAR(out, pageSize);
  BACKUP_SCALAR(out, prpContext.handledSize);
  BACKUP_SCALAR(out, prpContext.bufferSize);

  if (prpContext.bufferSize > 0) {
    BACKUP_BLOB(out, prpContext.buffer, prpContext.bufferSize);
  }

  uint64_t size = prpList.size();
  BACKUP_SCALAR(out, size);
  BACKUP_BLOB(out, prpList.data(), size * sizeof(PRP));
}

void PRPEngine::restoreCheckpoint(std::istream &in) noexcept {
  DMAEngine::restoreCheckpoint(in);

  RESTORE_SCALAR(in, inited);
  RESTORE_SCALAR(in, totalSize);
  RESTORE_SCALAR(in, pageSize);
  RESTORE_SCALAR(in, prpContext.handledSize);
  RESTORE_SCALAR(in, prpContext.bufferSize);

  if (prpContext.bufferSize > 0) {
    prpContext.buffer = (uint8_t *)malloc(prpContext.bufferSize);

    RESTORE_BLOB(in, prpContext.buffer, prpContext.bufferSize);
  }

  uint64_t size;
  RESTORE_SCALAR(in, size);

  prpList.resize(size);
  RESTORE_BLOB(in, prpList.data(), size * sizeof(PRP));
}

SGLEngine::SGLDescriptor::SGLDescriptor() {
  memset(data, 0, 16);
}

SGLEngine::Chunk::Chunk() : address(0), length(0), ignore(true) {}

SGLEngine::Chunk::Chunk(uint64_t a, uint32_t l, bool i)
    : address(a), length(l), ignore(i) {}

SGLEngine::SGLInitContext::SGLInitContext() : bufferSize(0), buffer(nullptr) {}

SGLEngine::SGLEngine(ObjectData &o, DMAInterface *i)
    : DMAEngine(o, i), inited(false), totalSize(0) {}

SGLEngine::~SGLEngine() {}

void SGLEngine::parseSGLDescriptor(SGLDescriptor &desc) {
  switch (SGL_TYPE(desc.id)) {
    case TYPE_DATA_BLOCK_DESCRIPTOR:
    case TYPE_KEYED_DATA_BLOCK_DESCRIPTOR:
      chunkList.push_back(Chunk(desc.address, desc.length, false));
      totalSize += desc.length;

      break;
    case TYPE_BIT_BUCKET_DESCRIPTOR:
      chunkList.push_back(Chunk(desc.address, desc.length, true));
      totalSize += desc.length;

      break;
    default:
      panic("Invalid SGL descriptor");
      break;
  }

  panic_if(SGL_SUBTYPE(desc.id) != SUBTYPE_ADDRESS, "Unexpected SGL subtype");
}

void SGLEngine::parseSGLSegment(uint64_t address, uint32_t length, Event eid) {
  // Allocate buffer
  dmaContext.eid = eid;
  sglContext.bufferSize = length;
  sglContext.buffer = (uint8_t *)calloc(length, 1);

  panic_if(sglContext.buffer == nullptr, "Out of memory.");

  // Read segment
  pInterface->read(address, length, sglContext.buffer, readSGL);
}

void SGLEngine::parseSGLSegment_readDone(uint64_t now) {
  SGLDescriptor desc;
  bool next = false;

  for (uint32_t i = 0; i < sglContext.bufferSize; i += 16) {
    memcpy(desc.data, sglContext.buffer + i, 16);

    switch (SGL_TYPE(desc.id)) {
      case TYPE_DATA_BLOCK_DESCRIPTOR:
      case TYPE_KEYED_DATA_BLOCK_DESCRIPTOR:
      case TYPE_BIT_BUCKET_DESCRIPTOR:
        parseSGLDescriptor(desc);

        break;
      case TYPE_SEGMENT_DESCRIPTOR:
      case TYPE_LAST_SEGMENT_DESCRIPTOR:
        next = true;

        panic_if(i != sglContext.bufferSize - 16, "Invalid SGL segment");

        break;
    }
  }

  free(sglContext.buffer);

  // Go to next
  if (next) {
    sglContext.bufferSize = desc.length;
    sglContext.buffer = (uint8_t *)calloc(desc.length, 1);

    panic_if(sglContext.buffer == nullptr, "Out of memory.");

    pInterface->read(desc.address, desc.length, sglContext.buffer, readSGL);
  }
  else {
    inited = true;

    schedule(dmaContext.eid, now);
  }
}

void SGLEngine::init(uint64_t prp1, uint64_t prp2, uint64_t, Event eid) {
  SGLDescriptor desc;

  // Create first SGL descriptor from PRP pointers
  memcpy(desc.data, &prp1, 8);
  memcpy(desc.data + 8, &prp2, 8);

  // Check type
  if (SGL_TYPE(desc.id) == TYPE_DATA_BLOCK_DESCRIPTOR ||
      SGL_TYPE(desc.id) == TYPE_KEYED_DATA_BLOCK_DESCRIPTOR) {
    inited = true;

    // This is entire buffer
    parseSGLDescriptor(desc);

    schedule(eid, getTick());
  }
  else if (SGL_TYPE(desc.id) == TYPE_SEGMENT_DESCRIPTOR ||
           SGL_TYPE(desc.id) == TYPE_LAST_SEGMENT_DESCRIPTOR) {
    // This points segment
    parseSGLSegment(desc.address, desc.length, eid);
  }
}

void SGLEngine::read(uint64_t offset, uint64_t length, uint8_t *buffer,
                     Event eid) {
  panic_if(!inited, "Accessed to uninitialized SGLEngine.");

  uint64_t totalRead = 0;
  uint64_t currentOffset = 0;
  uint64_t read;
  bool begin = false;

  dmaContext.eid = eid;
  dmaContext.counter = 0;

  for (auto &iter : chunkList) {
    if (begin) {
      read = MIN(iter.length, length - totalRead);

      if (!iter.ignore) {
        dmaContext.counter++;
        pInterface->read(iter.address, read, buffer ? buffer + totalRead : NULL,
                         dmaHandler);
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
        dmaContext.counter++;
        pInterface->read(iter.address + totalRead, read, buffer, dmaHandler);
      }

      totalRead = read;
    }

    currentOffset += iter.length;
  }

  if (dmaContext.counter == 0) {
    dmaContext.counter = 1;

    dmaDone(getTick());
  }
}

void SGLEngine::write(uint64_t offset, uint64_t length, uint8_t *buffer,
                      Event eid) {
  panic_if(!inited, "Accessed to uninitialized SGLEngine.");

  uint64_t totalWritten = 0;
  uint64_t currentOffset = 0;
  uint64_t written;
  bool begin = false;

  dmaContext.eid = eid;
  dmaContext.counter = 0;

  for (auto &iter : chunkList) {
    if (begin) {
      written = MIN(iter.length, length - totalWritten);

      if (!iter.ignore) {
        dmaContext.counter++;
        pInterface->write(iter.address, written,
                          buffer ? buffer + totalWritten : NULL, dmaHandler);
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
        dmaContext.counter++;
        pInterface->write(iter.address + totalWritten, written, buffer,
                          dmaHandler);
      }

      totalWritten = written;
    }

    currentOffset += iter.length;
  }

  if (dmaContext.counter == 0) {
    dmaContext.counter = 1;

    dmaDone(getTick());
  }
}

void SGLEngine::createCheckpoint(std::ostream &out) noexcept {
  DMAEngine::createCheckpoint(out);

  BACKUP_SCALAR(out, inited);
  BACKUP_SCALAR(out, totalSize);
  BACKUP_SCALAR(out, sglContext.bufferSize);

  if (sglContext.bufferSize > 0) {
    BACKUP_BLOB(out, sglContext.buffer, sglContext.bufferSize);
  }

  uint64_t size = chunkList.size();
  BACKUP_SCALAR(out, size);
  BACKUP_BLOB(out, chunkList.data(), size * sizeof(Chunk));
}

void SGLEngine::restoreCheckpoint(std::istream &in) noexcept {
  DMAEngine::restoreCheckpoint(in);

  RESTORE_SCALAR(in, inited);
  RESTORE_SCALAR(in, totalSize);
  RESTORE_SCALAR(in, sglContext.bufferSize);

  if (sglContext.bufferSize > 0) {
    sglContext.buffer = (uint8_t *)malloc(sglContext.bufferSize);

    RESTORE_BLOB(in, sglContext.buffer, sglContext.bufferSize);
  }

  uint64_t size;
  RESTORE_SCALAR(in, size);

  chunkList.resize(size);
  RESTORE_BLOB(in, chunkList.data(), size * sizeof(Chunk));
}

}  // namespace SimpleSSD::HIL::NVMe
