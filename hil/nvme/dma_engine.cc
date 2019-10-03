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

PRPEngine::PRPEngine(ObjectData &o, Interface *i, uint64_t p)
    : DMAEngine(o, i), inited(false), totalSize(0), pageSize(p) {
  panic_if(popcount(p) != 1, "Invalid memory page size provided.");

  readPRPList = createEvent(
      [this](uint64_t t, EventContext c) {
        getPRPListFromPRP_readDone(t, c.get<PRPInitContext *>());
      },
      "HIL::NVMe::PRPEngine::readPRPList");
}

PRPEngine::~PRPEngine() {}

void PRPEngine::getPRPListFromPRP_readDone(uint64_t now, PRPInitContext *data) {
  uint64_t listPRP;
  uint64_t listPRPSize;

  for (size_t i = 0; i < data->bufferSize; i += 8) {
    listPRP = *((uint64_t *)(data->buffer + i));
    listPRPSize = getSizeFromPRP(listPRP);

    if (listPRP == 0) {
      panic("prp_list: Invalid PRP in PRP List");
    }

    data->handledSize += listPRPSize;
    prpList.push_back(PRP(listPRP, listPRPSize));

    if (data->handledSize >= totalSize) {
      break;
    }
  }

  free(data->buffer);

  if (data->handledSize < totalSize) {
    // PRP list ends but size is not full
    // Last item of PRP list is pointer of another PRP list
    listPRP = prpList.back().address;
    listPRPSize = getSizeFromPRP(listPRP);

    prpList.pop_back();

    data->bufferSize = listPRPSize;
    data->buffer = (uint8_t *)malloc(data->bufferSize);

    panic_if(data->buffer == nullptr, "Out of memory.");

    pInterface->read(listPRP, data->bufferSize, data->buffer, readPRPList,
                     data);
  }
  else {
    // Done
    inited = true;

    schedule(data->eid, now, data->context);

    delete data;
  }
}

uint64_t PRPEngine::getSizeFromPRP(uint64_t prp) {
  return pageSize - (prp & (pageSize - 1));
}

void PRPEngine::getPRPListFromPRP(uint64_t prp, Event eid,
                                  EventContext context) {
  auto data = new PRPInitContext();

  data->handledSize = 0;
  data->eid = eid;
  data->context = context;
  data->bufferSize = getSizeFromPRP(prp);
  data->buffer = (uint8_t *)malloc(data->bufferSize);

  panic_if(data->buffer == nullptr, "Out of memory.");

  pInterface->read(prp, data->bufferSize, data->buffer, readPRPList, data);
}

void PRPEngine::initData(uint64_t prp1, uint64_t prp2, uint64_t sizeLimit,
                         Event eid, EventContext context) {
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
      getPRPListFromPRP(prp2, eid, context);

      break;
    default:
      panic("Invalid PRP1/2 while parsing.");

      break;
  }

  if (immediate) {
    inited = true;

    schedule(eid, getTick(), context);
  }
}

void PRPEngine::initQueue(uint64_t base, uint64_t size, bool cont, Event eid,
                          EventContext context) {
  totalSize = size;

  if (cont) {
    prpList.push_back(PRP(base, size));

    inited = true;
    schedule(eid, getTick(), context);
  }
  else {
    getPRPListFromPRP(base, eid, context);
  }
}

void PRPEngine::read(uint64_t offset, uint64_t length, uint8_t *buffer,
                     Event eid, EventContext context) {
  panic_if(!inited, "Accessed to uninitialized PRPEngine.");

  DMAContext *readContext = new DMAContext(eid, context);

  uint64_t totalRead = 0;
  uint64_t currentOffset = 0;
  uint64_t read;
  bool begin = false;

  for (auto &iter : prpList) {
    if (begin) {
      read = MIN(iter.size, length - totalRead);
      readContext->counter++;
      pInterface->read(iter.address, read, buffer ? buffer + totalRead : NULL,
                       dmaHandler, readContext);
      totalRead += read;

      if (totalRead == length) {
        break;
      }
    }

    if (!begin && currentOffset + iter.size > offset) {
      begin = true;
      totalRead = offset - currentOffset;
      read = MIN(iter.size - totalRead, length);
      readContext->counter++;
      pInterface->read(iter.address + totalRead, read, buffer, dmaHandler,
                       readContext);
      totalRead = read;
    }

    currentOffset += iter.size;
  }

  if (readContext->counter == 0) {
    readContext->counter = 1;

    dmaDone(getTick(), readContext);
  }
}

void PRPEngine::write(uint64_t offset, uint64_t length, uint8_t *buffer,
                      Event eid, EventContext context) {
  panic_if(!inited, "Accessed to uninitialized PRPEngine.");

  DMAContext *writeContext = new DMAContext(eid, context);
  uint64_t totalWritten = 0;
  uint64_t currentOffset = 0;
  uint64_t written;
  bool begin = false;

  for (auto &iter : prpList) {
    if (begin) {
      written = MIN(iter.size, length - totalWritten);
      writeContext->counter++;
      pInterface->write(iter.address, written,
                        buffer ? buffer + totalWritten : NULL, dmaHandler,
                        writeContext);
      totalWritten += written;

      if (totalWritten == length) {
        break;
      }
    }

    if (!begin && currentOffset + iter.size > offset) {
      begin = true;
      totalWritten = offset - currentOffset;
      written = MIN(iter.size - totalWritten, length);
      writeContext->counter++;
      pInterface->write(iter.address + totalWritten, written, buffer,
                        dmaHandler, writeContext);
      totalWritten = written;
    }

    currentOffset += iter.size;
  }

  if (writeContext->counter == 0) {
    writeContext->counter = 1;

    dmaDone(getTick(), writeContext);
  }
}

void PRPEngine::createCheckpoint(std::ostream &out) noexcept {
  DMAEngine::createCheckpoint(out);

  BACKUP_SCALAR(out, inited);
  BACKUP_SCALAR(out, totalSize);
  BACKUP_SCALAR(out, pageSize);

  uint64_t size = prpList.size();
  BACKUP_SCALAR(out, size);
  BACKUP_BLOB(out, prpList.data(), size * sizeof(PRP));
}

void PRPEngine::restoreCheckpoint(std::istream &in) noexcept {
  DMAEngine::restoreCheckpoint(in);

  RESTORE_SCALAR(in, inited);
  RESTORE_SCALAR(in, totalSize);
  RESTORE_SCALAR(in, pageSize);

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

SGLEngine::SGLEngine(ObjectData &o, Interface *i)
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

void SGLEngine::parseSGLSegment(uint64_t address, uint32_t length, Event eid,
                                EventContext context) {
  auto data = new SGLInitContext();

  // Allocate buffer
  data->eid = eid;
  data->context = context;
  data->bufferSize = length;
  data->buffer = (uint8_t *)calloc(length, 1);

  panic_if(data->buffer == nullptr, "Out of memory.");

  // Read segment
  pInterface->read(address, length, data->buffer, readSGL, data);
}

void SGLEngine::parseSGLSegment_readDone(uint64_t now, SGLInitContext *data) {
  SGLDescriptor desc;
  bool next = false;

  for (uint32_t i = 0; i < data->bufferSize; i += 16) {
    memcpy(desc.data, data->buffer + i, 16);

    switch (SGL_TYPE(desc.id)) {
      case TYPE_DATA_BLOCK_DESCRIPTOR:
      case TYPE_KEYED_DATA_BLOCK_DESCRIPTOR:
      case TYPE_BIT_BUCKET_DESCRIPTOR:
        parseSGLDescriptor(desc);

        break;
      case TYPE_SEGMENT_DESCRIPTOR:
      case TYPE_LAST_SEGMENT_DESCRIPTOR:
        next = true;

        panic_if(i != data->bufferSize - 16, "Invalid SGL segment");

        break;
    }
  }

  free(data->buffer);

  // Go to next
  if (next) {
    data->bufferSize = desc.length;
    data->buffer = (uint8_t *)calloc(desc.length, 1);

    panic_if(data->buffer == nullptr, "Out of memory.");

    pInterface->read(desc.address, desc.length, data->buffer, readSGL, data);
  }
  else {
    inited = true;

    schedule(data->eid, now, data->context);

    delete data;
  }
}

void SGLEngine::init(uint64_t prp1, uint64_t prp2, Event eid,
                     EventContext context) {
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

    schedule(eid, getTick(), context);
  }
  else if (SGL_TYPE(desc.id) == TYPE_SEGMENT_DESCRIPTOR ||
           SGL_TYPE(desc.id) == TYPE_LAST_SEGMENT_DESCRIPTOR) {
    // This points segment
    parseSGLSegment(desc.address, desc.length, eid, context);
  }
}

void SGLEngine::read(uint64_t offset, uint64_t length, uint8_t *buffer,
                     Event eid, EventContext context) {
  panic_if(!inited, "Accessed to uninitialized SGLEngine.");

  DMAContext *readContext = new DMAContext(eid, context);
  uint64_t totalRead = 0;
  uint64_t currentOffset = 0;
  uint64_t read;
  bool begin = false;

  for (auto &iter : chunkList) {
    if (begin) {
      read = MIN(iter.length, length - totalRead);

      if (!iter.ignore) {
        readContext->counter++;
        pInterface->read(iter.address, read, buffer ? buffer + totalRead : NULL,
                         dmaHandler, readContext);
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
        readContext->counter++;
        pInterface->read(iter.address + totalRead, read, buffer, dmaHandler,
                         readContext);
      }

      totalRead = read;
    }

    currentOffset += iter.length;
  }

  if (readContext->counter == 0) {
    readContext->counter = 1;

    dmaDone(getTick(), readContext);
  }
}

void SGLEngine::write(uint64_t offset, uint64_t length, uint8_t *buffer,
                      Event eid, EventContext context) {
  panic_if(!inited, "Accessed to uninitialized SGLEngine.");

  DMAContext *writeContext = new DMAContext(eid, context);
  uint64_t totalWritten = 0;
  uint64_t currentOffset = 0;
  uint64_t written;
  bool begin = false;

  for (auto &iter : chunkList) {
    if (begin) {
      written = MIN(iter.length, length - totalWritten);

      if (!iter.ignore) {
        writeContext->counter++;
        pInterface->write(iter.address, written,
                          buffer ? buffer + totalWritten : NULL, dmaHandler,
                          writeContext);
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
        writeContext->counter++;
        pInterface->write(iter.address + totalWritten, written, buffer,
                          dmaHandler, writeContext);
      }

      totalWritten = written;
    }

    currentOffset += iter.length;
  }

  if (writeContext->counter == 0) {
    writeContext->counter = 1;

    dmaDone(getTick(), writeContext);
  }
}

void SGLEngine::createCheckpoint(std::ostream &out) noexcept {
  DMAEngine::createCheckpoint(out);

  BACKUP_SCALAR(out, inited);
  BACKUP_SCALAR(out, totalSize);

  uint64_t size = chunkList.size();
  BACKUP_SCALAR(out, size);
  BACKUP_BLOB(out, chunkList.data(), size * sizeof(Chunk));
}

void SGLEngine::restoreCheckpoint(std::istream &in) noexcept {
  DMAEngine::restoreCheckpoint(in);

  RESTORE_SCALAR(in, inited);
  RESTORE_SCALAR(in, totalSize);

  uint64_t size;
  RESTORE_SCALAR(in, size);

  chunkList.resize(size);
  RESTORE_BLOB(in, chunkList.data(), size * sizeof(Chunk));
}

}  // namespace SimpleSSD::HIL::NVMe
