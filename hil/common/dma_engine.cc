// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "hil/common/dma_engine.hh"

namespace SimpleSSD::HIL {

enum SGLDescriptorType : uint8_t {
  DataBlock = 0x00,
  BitBucket = 0x01,
  Segment = 0x02,
  LastSegment = 0x03,
  KeyedDataBlock = 0x04,
};

enum SGLDescriptorSubtype : uint8_t {
  Address = 0x00,
  Offset = 0x01,
  TransportSpecific = 0x02,
};

//! PRDT structure definition
union PRDT {
  uint8_t data[16];
  struct {
    uint64_t address;
    uint32_t rsvd1;
    uint32_t size : 22;
    uint32_t rsvd2 : 9;
    uint32_t intr : 1;
  };
};

//! SGL descriptor structure definition
union SGLDescriptor {
  uint8_t data[16];
  struct {
    uint64_t address;
    uint32_t length;
    uint8_t reserved[3];
    uint8_t id;
  };
  struct {
    uint64_t dptr1;
    uint64_t dptr2;
  };

  SGLDescriptorType getType() { return (SGLDescriptorType)(id >> 4); }

  SGLDescriptorSubtype getSubtype() {
    return (SGLDescriptorSubtype)(id & 0x0F);
  }
};

PhysicalRegion::PhysicalRegion() : address(0), size(0), ignore(true) {}

PhysicalRegion::PhysicalRegion(uint64_t a, uint32_t s)
    : address(a), size(s), ignore(false) {}

PhysicalRegion::PhysicalRegion(uint64_t a, uint32_t s, bool i)
    : address(a), size(s), ignore(i) {}

DMAData::DMAData()
    : eid(InvalidEventID),
      counter(0),
      handledSize(0),
      requestedSize(0),
      bufferSize(0),
      buffer(nullptr) {}

DMAData::~DMAData() {
  if (bufferSize > 0) {
    free(buffer);
  }
}

void DMAData::allocateBuffer(uint64_t size) {
  bufferSize = size;
  buffer = (uint8_t *)calloc(size, 1);
}

void DMAData::deallocateBuffer() {
  free(buffer);

  bufferSize = 0;
  buffer = nullptr;
}

DMAEngine::DMAEngine(ObjectData &o, DMAInterface *i)
    : Object(o), interface(i), pageSize(0) {
  eventDMADone = createEvent([this](uint64_t, uint64_t) { dmaDone(); },
                             "HIL::DMAEngine::dmaHandler");
  eventPRDTInitDone =
      createEvent([this](uint64_t, uint64_t) { prdt_readDone(); },
                  "HIL::DMAEngine::eventPRDTInitDone");
  eventPRPReadDone =
      createEvent([this](uint64_t, uint64_t) { getPRPListFromPRP_readDone(); },
                  "HIL::DMAEngine::eventPRPReadDone");
  eventSGLReadDone =
      createEvent([this](uint64_t, uint64_t) { parseSGLSegment_readDone(); },
                  "HIL::DMAEngine::eventSGLReadDone");
}

DMAEngine::~DMAEngine() {
  panic_if(tagList.size() > 0, "Not all DMA tag is released.");
}

void DMAEngine::dmaDone() {
  auto tag = pendingTagList.front();

  tag->counter--;

  if (tag->counter == 0) {
    pendingTagList.pop_front();

    schedule(tag->eid, tag->data);
  }
}

DMATag DMAEngine::createTag() {
  auto tag = new DMAData();

  tagList.emplace(tag);

  return tag;
}

void DMAEngine::destroyTag(DMATag tag) {
  auto iter = tagList.find(tag);

  if (iter != tagList.end()) {
    tagList.erase(iter);
  }

  delete tag;
}

void DMAEngine::prdt_readDone() {
  auto tag = pendingTagList.front();

  pendingTagList.pop_front();

  // Fill vector
  PRDT *prdt = (PRDT *)tag->buffer;
  uint32_t entryCount = (uint32_t)(tag->bufferSize / sizeof(PRDT));

  for (uint32_t i = 0; i < entryCount; i++) {
    tag->prList.push_back(PhysicalRegion(prdt->address, prdt->size + 1));
    tag->handledSize += prdt->size + 1;
  }

  tag->deallocateBuffer();

  schedule(tag->eid, tag->data);
}

uint32_t DMAEngine::getPRPSize(uint64_t prp) {
  return pageSize - (prp & (pageSize - 1));
}

void DMAEngine::getPRPListFromPRP(DMATag tag, uint64_t prp) {
  tag->allocateBuffer(getPRPSize(prp));
  pendingTagList.push_back(tag);

  interface->read(prp, tag->bufferSize, tag->buffer, eventPRPReadDone);
}

void DMAEngine::getPRPListFromPRP_readDone() {
  auto tag = pendingTagList.front();
  uint64_t listPRP;
  uint64_t listPRPSize;

  for (size_t i = 0; i < tag->bufferSize; i += 8) {
    listPRP = *((uint64_t *)(tag->buffer + i));
    listPRPSize = getPRPSize(listPRP);

    if (listPRP == 0) {
      panic("Invalid PRP in PRP List");
    }

    tag->prList.push_back(PhysicalRegion(listPRP, listPRPSize));
    tag->handledSize += listPRPSize;

    if (tag->handledSize >= tag->requestedSize) {
      break;
    }
  }

  tag->deallocateBuffer();

  if (tag->handledSize < tag->requestedSize) {
    // PRP list ends but size is not full
    // Last item of PRP list is pointer of another PRP list
    listPRP = tag->prList.back().address;

    tag->prList.pop_back();

    getPRPListFromPRP(tag, listPRP);
  }
  else {
    schedule(tag->eid, tag->data);
  }
}

void DMAEngine::parseSGLDescriptor(DMATag tag, SGLDescriptor *desc) {
  switch (desc->getType()) {
    case DataBlock:
    case KeyedDataBlock:
      tag->prList.push_back(PhysicalRegion(desc->address, desc->length));
      tag->handledSize += desc->length;

      break;
    case BitBucket:
      tag->prList.push_back(PhysicalRegion(desc->address, desc->length, true));
      tag->handledSize += desc->length;

      break;
    default:
      panic("Invalid SGL descriptor");
      break;
  }

  panic_if(desc->getSubtype() != SGLDescriptorSubtype::Address,
           "Unexpected SGL subtype");
}

void DMAEngine::parseSGLSegment(DMATag tag, uint64_t address, uint32_t length) {
  tag->allocateBuffer(length);
  pendingTagList.push_back(tag);

  interface->read(address, tag->bufferSize, tag->buffer, eventSGLReadDone);
}

void DMAEngine::parseSGLSegment_readDone() {
  auto tag = pendingTagList.front();
  bool next = false;

  SGLDescriptor *desc;

  for (uint32_t i = 0; i < tag->bufferSize; i += 16) {
    desc = (SGLDescriptor *)(tag->buffer + i * 16);

    switch (desc->getType()) {
      case DataBlock:
      case KeyedDataBlock:
      case BitBucket:
        parseSGLDescriptor(tag, desc);

        break;
      case Segment:
      case LastSegment:
        next = true;

        // Segment must appear at the end of buffer
        panic_if(i + 16 != tag->bufferSize, "Invalid SGL segment");

        break;
    }
  }

  tag->deallocateBuffer();

  // Go to next
  if (next) {
    parseSGLSegment(tag, desc->address, desc->length);
  }
  else {
    schedule(tag->eid, tag->data);
  }
}

void DMAEngine::updatePageSize(uint64_t size) {
  pageSize = size;
}

DMATag DMAEngine::initFromPRDT(uint64_t base, uint32_t size, Event eid,
                               uint64_t data) noexcept {
  auto ret = createTag();

  // PRDT is continuous list
  size *= sizeof(PRDT);

  // Prepare for PRDT read
  ret->eid = eid;
  ret->data = data;
  ret->handledSize = 0;
  ret->requestedSize = 0;  // No requested size in PRDT, reuse it
  ret->allocateBuffer(size);
  pendingTagList.push_back(ret);

  interface->read(base, size, ret->buffer, eventPRDTInitDone);

  return ret;
}

DMATag DMAEngine::initFromPRP(uint64_t prp1, uint64_t prp2, uint32_t size,
                              Event eid, uint64_t data) noexcept {
  panic_if(popcount64(pageSize) != 1, "Invalid memory page size provided.");

  auto ret = createTag();

  bool immediate = true;
  uint8_t mode = 0xFF;
  uint64_t prp1Size = getPRPSize(prp1);
  uint64_t prp2Size = getPRPSize(prp2);

  ret->handledSize = 0;
  ret->requestedSize = size;

  // Determine PRP1 and PRP2
  if (prp1 == 0) {
    // This is non-continuous NVMe queue
    mode = 3;
  }
  else if (ret->requestedSize <= pageSize) {
    if (ret->requestedSize <= prp1Size) {
      mode = 0;
    }
    else {
      mode = 1;
    }
  }
  else if (ret->requestedSize <= pageSize * 2) {
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
      ret->prList.push_back(PhysicalRegion(prp1, prp1Size));
      ret->handledSize = prp1Size;

      break;
    case 1:
      // PRP1 and PRP2 are PRP pointer
      ret->prList.push_back(PhysicalRegion(prp1, prp1Size));
      ret->prList.push_back(PhysicalRegion(prp2, prp2Size));

      panic_if(prp1Size + prp2Size < ret->requestedSize, "Invalid DPTR size");

      ret->requestedSize = prp1Size + prp2Size;

      break;
    case 2:
      // PRP1 is PRP pointer, PRP2 is PRP list
      ret->prList.push_back(PhysicalRegion(prp1, prp1Size));

      /* fallthrough */
    case 3:
      // Prepare for read
      immediate = false;
      ret->eid = eid;
      ret->data = data;
      ret->handledSize = prp1Size;

      getPRPListFromPRP(ret, prp2);

      break;
    default:
      panic("Invalid PRP1/2 while parsing.");

      break;
  }

  if (immediate) {
    schedule(eid, data);
  }

  return ret;
}

DMATag DMAEngine::initFromSGL(uint64_t dptr1, uint64_t dptr2, uint32_t size,
                              Event eid, uint64_t data) noexcept {
  auto ret = createTag();

  SGLDescriptor desc;

  ret->requestedSize = size;

  // Create first SGL descriptor from PRP pointers
  desc.dptr1 = dptr1;
  desc.dptr2 = dptr2;

  // Check type
  if (desc.getType() == SGLDescriptorType::DataBlock ||
      desc.getType() == SGLDescriptorType::KeyedDataBlock) {
    // This is entire buffer
    parseSGLDescriptor(ret, &desc);

    schedule(eid, data);
  }
  else if (desc.getType() == SGLDescriptorType::Segment ||
           desc.getType() == SGLDescriptorType::LastSegment) {
    ret->eid = eid;
    ret->data = data;

    parseSGLSegment(ret, desc.address, desc.length);
  }
  else {
    panic("Unexpected SGL descriptor type.");
  }
}

DMATag DMAEngine::initRaw(uint64_t base, uint32_t size) noexcept {
  auto ret = createTag();

  ret->handledSize = 0;
  ret->requestedSize = size;
  ret->eid = InvalidEventID;
  ret->prList.push_back(PhysicalRegion(base, size));

  return ret;
}

void DMAEngine::deinit(DMATag tag) noexcept {
  destroyTag(tag);
}

void DMAEngine::read(DMATag tag, uint64_t offset, uint32_t size,
                     uint8_t *buffer, Event eid, uint64_t data) noexcept {
  panic_if(!tag, "Accessed to uninitialized DMAEngine.");

  uint64_t totalRead = 0;
  uint64_t currentOffset = 0;
  uint64_t read;
  bool begin = false;

  tag->eid = eid;
  tag->data = data;
  tag->counter = 0;

  for (auto &iter : tag->prList) {
    if (begin) {
      read = MIN(iter.size, size - totalRead);

      if (!iter.ignore) {
        tag->counter++;

        interface->read(iter.address, read, buffer ? buffer + totalRead : NULL,
                        eventDMADone);
      }

      totalRead += read;

      if (totalRead == size) {
        break;
      }
    }

    if (!begin && currentOffset + iter.size > offset) {
      begin = true;
      totalRead = offset - currentOffset;
      read = MIN(iter.size - totalRead, size);

      if (!iter.ignore) {
        tag->counter++;

        interface->read(iter.address + totalRead, read, buffer, eventDMADone);
      }

      totalRead = read;
    }

    currentOffset += iter.size;
  }

  pendingTagList.push_back(tag);

  if (tag->counter == 0) {
    tag->counter = 1;

    dmaDone();
  }
}

void DMAEngine::write(DMATag tag, uint64_t offset, uint32_t size,
                      uint8_t *buffer, Event eid, uint64_t data) noexcept {
  panic_if(!tag, "Accessed to uninitialized DMAEngine.");

  uint64_t totalWritten = 0;
  uint64_t currentOffset = 0;
  uint64_t written;
  bool begin = false;

  tag->eid = eid;
  tag->data = data;
  tag->counter = 0;

  for (auto &iter : tag->prList) {
    if (begin) {
      written = MIN(iter.size, size - totalWritten);

      if (!iter.ignore) {
        tag->counter++;

        interface->write(iter.address, written,
                         buffer ? buffer + totalWritten : NULL, eventDMADone);
      }

      totalWritten += written;

      if (totalWritten == size) {
        break;
      }
    }

    if (!begin && currentOffset + iter.size > offset) {
      begin = true;
      totalWritten = offset - currentOffset;
      written = MIN(iter.size - totalWritten, size);

      if (!iter.ignore) {
        tag->counter++;

        interface->write(iter.address + totalWritten, written, buffer,
                         eventDMADone);
      }

      totalWritten = written;
    }

    currentOffset += iter.size;
  }

  pendingTagList.push_back(tag);

  if (tag->counter == 0) {
    tag->counter = 1;

    dmaDone();
  }
}

void DMAEngine::getStatList(std::vector<Stat> &, std::string) noexcept {}

void DMAEngine::getStatValues(std::vector<double> &) noexcept {}

void DMAEngine::resetStatValues() noexcept {}

void DMAEngine::createCheckpoint(std::ostream &out) const noexcept {
  BACKUP_EVENT(out, eventDMADone);
  BACKUP_EVENT(out, eventPRDTInitDone);
  BACKUP_EVENT(out, eventPRPReadDone);
  BACKUP_EVENT(out, eventSGLReadDone);

  uint64_t size = tagList.size();
  BACKUP_SCALAR(out, size);

  for (auto &iter : tagList) {
    BACKUP_DMATAG(out, iter);

    BACKUP_EVENT(out, iter->eid);
    BACKUP_SCALAR(out, iter->counter);

    BACKUP_SCALAR(out, iter->handledSize);
    BACKUP_SCALAR(out, iter->requestedSize);
    BACKUP_SCALAR(out, iter->bufferSize);

    if (iter->bufferSize) {
      BACKUP_BLOB(out, iter->buffer, iter->bufferSize);
    }

    uint64_t size = iter->prList.size();
    BACKUP_SCALAR(out, size);

    for (auto &pr : iter->prList) {
      BACKUP_SCALAR(out, pr.address);
      BACKUP_SCALAR(out, pr.size);
      BACKUP_SCALAR(out, pr.ignore);
    }
  }

  size = pendingTagList.size();
  BACKUP_SCALAR(out, size);

  for (auto &iter : pendingTagList) {
    BACKUP_DMATAG(out, iter);
  }
}

void DMAEngine::restoreCheckpoint(std::istream &in) noexcept {
  RESTORE_EVENT(in, eventDMADone);
  RESTORE_EVENT(in, eventPRDTInitDone);
  RESTORE_EVENT(in, eventPRPReadDone);
  RESTORE_EVENT(in, eventSGLReadDone);

  uint64_t size;
  DMATag oldTag, newTag;

  RESTORE_SCALAR(in, size);

  for (uint64_t i = 0; i < size; i++) {
    newTag = new DMAData();

    RESTORE_SCALAR(in, oldTag);

    RESTORE_EVENT(in, newTag->eid);
    RESTORE_SCALAR(in, newTag->counter);

    RESTORE_SCALAR(in, newTag->handledSize);
    RESTORE_SCALAR(in, newTag->requestedSize);
    RESTORE_SCALAR(in, newTag->bufferSize);

    if (newTag->bufferSize) {
      RESTORE_BLOB(in, newTag->buffer, newTag->bufferSize);
    }

    uint64_t list;
    RESTORE_SCALAR(in, list);

    for (uint64_t j = 0; j < list; j++) {
      PhysicalRegion pr;

      RESTORE_SCALAR(in, pr.address);
      RESTORE_SCALAR(in, pr.size);
      RESTORE_SCALAR(in, pr.ignore);

      newTag->prList.emplace_back(pr);
    }

    oldTagList.emplace(std::make_pair(oldTag, newTag));
  }

  RESTORE_SCALAR(in, size);

  for (uint64_t i = 0; i < size; i++) {
    RESTORE_SCALAR(in, oldTag);
    oldTag = restoreDMATag(oldTag);

    pendingTagList.push_back(oldTag);
  }
}

DMATag DMAEngine::restoreDMATag(DMATag oldTag) noexcept {
  if (oldTag == InvalidDMATag) {
    return InvalidDMATag;
  }

  auto iter = oldTagList.find(oldTag);

  if (iter == oldTagList.end()) {
    panic_log("Tag not found");
  }

  return iter->second;
}

void DMAEngine::clearOldDMATagList() noexcept {
  oldTagList.clear();
}

}  // namespace SimpleSSD::HIL
