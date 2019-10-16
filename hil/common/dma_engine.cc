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

DMAData::DMAData() {}

DMAEngine::DMASession::DMASession(DMATag t, Event e)
    : parent(t),
      eid(e),
      data(0),
      read(false),
      handled(0),
      requested(0),
      bufferSize(0),
      buffer(nullptr) {}

DMAEngine::DMASession::DMASession(DMATag t, Event e, uint64_t d)
    : parent(t),
      eid(e),
      data(d),
      read(false),
      handled(0),
      requested(0),
      bufferSize(0),
      buffer(nullptr) {}

DMAEngine::DMASession::DMASession(DMATag t, Event e, uint64_t d, bool r,
                                  uint64_t s, uint8_t *b)
    : parent(t),
      eid(e),
      data(d),
      read(r),
      handled(0),
      requested(s),
      bufferSize(0),
      buffer(b) {}

void DMAEngine::DMASession::allocateBuffer(uint64_t size) {
  bufferSize = size;
  buffer = (uint8_t *)calloc(size, 1);
}

void DMAEngine::DMASession::deallocateBuffer() {
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
  auto session = std::move(pendingTagList.front());

  pendingTagList.pop_front();

  if (session.handled == session.requested) {
    scheduleNow(session.eid, session.data);
  }
  else {
    if (session.read) {
      readNext(std::move(session));
    }
    else {
      writeNext(std::move(session));
    }
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
  auto session = std::move(pendingInitTagList.front());

  pendingInitTagList.pop_front();

  // Fill vector
  PRDT *prdt = (PRDT *)session.buffer;
  uint32_t entryCount = (uint32_t)(session.bufferSize / sizeof(PRDT));

  for (uint32_t i = 0; i < entryCount; i++) {
    session.parent->prList.push_back(
        PhysicalRegion(prdt->address, prdt->size + 1));
    session.handled += prdt->size + 1;
  }

  session.deallocateBuffer();

  scheduleNow(session.eid, session.data);
}

uint32_t DMAEngine::getPRPSize(uint64_t prp) {
  return pageSize - (prp & (pageSize - 1));
}

void DMAEngine::getPRPListFromPRP(DMASession &&session, uint64_t prp) {
  session.allocateBuffer(getPRPSize(prp));
  pendingInitTagList.emplace_back(session);

  interface->read(prp, session.bufferSize, session.buffer, eventPRPReadDone);
}

void DMAEngine::getPRPListFromPRP_readDone() {
  auto session = std::move(pendingInitTagList.front());
  uint64_t listPRP;
  uint64_t listPRPSize;

  pendingInitTagList.pop_front();

  for (size_t i = 0; i < session.bufferSize; i += 8) {
    listPRP = *((uint64_t *)(session.buffer + i));
    listPRPSize = getPRPSize(listPRP);

    if (listPRP == 0) {
      panic("Invalid PRP in PRP List");
    }

    session.parent->prList.push_back(PhysicalRegion(listPRP, listPRPSize));
    session.handled += listPRPSize;

    if (session.handled >= session.requested) {
      break;
    }
  }

  session.deallocateBuffer();

  if (session.handled < session.requested) {
    // PRP list ends but size is not full
    // Last item of PRP list is pointer of another PRP list
    listPRP = session.parent->prList.back().address;

    session.parent->prList.pop_back();

    getPRPListFromPRP(std::move(session), listPRP);
  }
  else {
    scheduleNow(session.eid, session.data);
  }
}

void DMAEngine::parseSGLDescriptor(DMASession &session, SGLDescriptor *desc) {
  switch (desc->getType()) {
    case DataBlock:
    case KeyedDataBlock:
      session.parent->prList.push_back(
          PhysicalRegion(desc->address, desc->length));
      session.handled += desc->length;

      break;
    case BitBucket:
      session.parent->prList.push_back(
          PhysicalRegion(desc->address, desc->length, true));
      session.handled += desc->length;

      break;
    default:
      panic("Invalid SGL descriptor");
      break;
  }

  panic_if(desc->getSubtype() != SGLDescriptorSubtype::Address,
           "Unexpected SGL subtype");
}

void DMAEngine::parseSGLSegment(DMASession &&session, uint64_t address,
                                uint32_t length) {
  session.allocateBuffer(length);
  pendingInitTagList.emplace_back(session);

  interface->read(address, session.bufferSize, session.buffer,
                  eventSGLReadDone);
}

void DMAEngine::parseSGLSegment_readDone() {
  auto session = std::move(pendingInitTagList.front());
  bool next = false;

  pendingInitTagList.pop_front();

  SGLDescriptor *desc;

  for (uint32_t i = 0; i < session.bufferSize; i += 16) {
    desc = (SGLDescriptor *)(session.buffer + i * 16);

    switch (desc->getType()) {
      case DataBlock:
      case KeyedDataBlock:
      case BitBucket:
        parseSGLDescriptor(session, desc);

        break;
      case Segment:
      case LastSegment:
        next = true;

        // Segment must appear at the end of buffer
        panic_if(i + 16 != session.bufferSize, "Invalid SGL segment");

        break;
    }
  }

  session.deallocateBuffer();

  // Go to next
  if (next) {
    parseSGLSegment(std::move(session), desc->address, desc->length);
  }
  else {
    scheduleNow(session.eid, session.data);
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
  DMASession session(ret, eid, data, false, size, nullptr);

  session.allocateBuffer(size);
  pendingInitTagList.emplace_back(session);

  interface->read(base, size, session.buffer, eventPRDTInitDone);

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

  DMASession session(ret, eid, data, false, size, nullptr);

  // Determine PRP1 and PRP2
  if (prp1 == 0) {
    // This is non-continuous NVMe queue
    mode = 3;
  }
  else if (session.requested <= pageSize) {
    if (session.requested <= prp1Size) {
      mode = 0;
    }
    else {
      mode = 1;
    }
  }
  else if (session.requested <= pageSize * 2) {
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
      session.handled = prp1Size;

      break;
    case 1:
      // PRP1 and PRP2 are PRP pointer
      ret->prList.push_back(PhysicalRegion(prp1, prp1Size));
      ret->prList.push_back(PhysicalRegion(prp2, prp2Size));

      panic_if(prp1Size + prp2Size < session.requested, "Invalid DPTR size");

      session.requested = prp1Size + prp2Size;

      break;
    case 2:
      // PRP1 is PRP pointer, PRP2 is PRP list
      ret->prList.push_back(PhysicalRegion(prp1, prp1Size));

      /* fallthrough */
    case 3:
      // Prepare for read
      immediate = false;
      session.handled = prp1Size;

      getPRPListFromPRP(std::move(session), prp2);

      break;
    default:
      panic("Invalid PRP1/2 while parsing.");

      break;
  }

  if (immediate) {
    scheduleNow(eid, data);
  }

  return ret;
}

DMATag DMAEngine::initFromSGL(uint64_t dptr1, uint64_t dptr2, uint32_t size,
                              Event eid, uint64_t data) noexcept {
  auto ret = createTag();

  SGLDescriptor desc;

  DMASession session(ret, eid, data, false, size, nullptr);

  // Create first SGL descriptor from PRP pointers
  desc.dptr1 = dptr1;
  desc.dptr2 = dptr2;

  // Check type
  if (desc.getType() == SGLDescriptorType::DataBlock ||
      desc.getType() == SGLDescriptorType::KeyedDataBlock) {
    // This is entire buffer
    parseSGLDescriptor(session, &desc);

    scheduleNow(eid, data);
  }
  else if (desc.getType() == SGLDescriptorType::Segment ||
           desc.getType() == SGLDescriptorType::LastSegment) {
    parseSGLSegment(std::move(session), desc.address, desc.length);
  }
  else {
    panic("Unexpected SGL descriptor type.");
  }

  return ret;
}

DMATag DMAEngine::initRaw(uint64_t base, uint32_t size) noexcept {
  auto ret = createTag();

  ret->prList.push_back(PhysicalRegion(base, size));

  return ret;
}

void DMAEngine::deinit(DMATag tag) noexcept {
  destroyTag(tag);
}

void DMAEngine::readNext(DMASession &&session) noexcept {
  uint64_t read;
  bool submit = false;

  auto iter = session.parent->prList.at(++session.regionIndex);

  read = MIN(iter.size, session.requested - session.handled);

  if (!iter.ignore) {
    submit = true;

    interface->read(iter.address, read,
                    session.buffer ? session.buffer + session.handled : nullptr,
                    eventDMADone);
  }

  session.handled += read;

  pendingTagList.emplace_back(session);

  if (!submit) {
    dmaDone();
  }
}

void DMAEngine::read(DMATag tag, uint64_t offset, uint32_t size,
                     uint8_t *buffer, Event eid, uint64_t data) noexcept {
  panic_if(!tag, "Accessed to uninitialized DMAEngine.");

  uint64_t currentOffset = 0;
  uint64_t read;
  bool submit = false;

  DMASession session(tag, eid, data, true, size, buffer);

  for (session.regionIndex = 0; session.regionIndex < tag->prList.size();
       session.regionIndex++) {
    auto iter = tag->prList.at(session.regionIndex);

    if (currentOffset + iter.size > offset) {
      session.handled = offset - currentOffset;

      read = MIN(iter.size - session.handled, size);

      if (!iter.ignore) {
        interface->read(iter.address + session.handled, read, buffer,
                        eventDMADone);

        submit = true;
      }

      session.handled = read;

      break;
    }

    currentOffset += iter.size;
  }

  pendingTagList.emplace_back(session);

  if (!submit) {
    dmaDone();
  }
}

void DMAEngine::writeNext(DMASession &&session) noexcept {
  uint64_t written;
  bool submit = false;

  auto iter = session.parent->prList.at(++session.regionIndex);

  written = MIN(iter.size, session.requested - session.handled);

  if (!iter.ignore) {
    submit = true;

    interface->write(
        iter.address, written,
        session.buffer ? session.buffer + session.handled : nullptr,
        eventDMADone);
  }

  session.handled += written;

  pendingTagList.emplace_back(session);

  if (!submit) {
    dmaDone();
  }
}

void DMAEngine::write(DMATag tag, uint64_t offset, uint32_t size,
                      uint8_t *buffer, Event eid, uint64_t data) noexcept {
  panic_if(!tag, "Accessed to uninitialized DMAEngine.");

  uint64_t currentOffset = 0;
  uint64_t written;
  bool submit = false;

  DMASession session(tag, eid, data, false, size, buffer);

  for (session.regionIndex = 0; session.regionIndex < tag->prList.size();
       session.regionIndex++) {
    auto iter = tag->prList.at(session.regionIndex);
    if (currentOffset + iter.size > offset) {
      session.handled = offset - currentOffset;

      written = MIN(iter.size - session.handled, size);

      if (!iter.ignore) {
        submit = true;

        interface->write(iter.address + session.handled, written, buffer,
                         eventDMADone);
      }

      session.handled = written;

      break;
    }

    currentOffset += iter.size;
  }

  pendingTagList.emplace_back(session);

  if (!submit) {
    dmaDone();
  }
}

void DMAEngine::getStatList(std::vector<Stat> &, std::string) noexcept {}

void DMAEngine::getStatValues(std::vector<double> &) noexcept {}

void DMAEngine::resetStatValues() noexcept {}

void DMAEngine::createCheckpoint(std::ostream &out) const noexcept {
  bool exist;

  BACKUP_EVENT(out, eventDMADone);
  BACKUP_EVENT(out, eventPRDTInitDone);
  BACKUP_EVENT(out, eventPRPReadDone);
  BACKUP_EVENT(out, eventSGLReadDone);

  uint64_t size = tagList.size();
  BACKUP_SCALAR(out, size);

  for (auto &iter : tagList) {
    BACKUP_DMATAG(out, iter);

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
    BACKUP_DMATAG(out, iter.parent);
    BACKUP_EVENT(out, iter.eid);
    BACKUP_SCALAR(out, iter.data);

    BACKUP_SCALAR(out, iter.read);
    BACKUP_SCALAR(out, iter.handled);
    BACKUP_SCALAR(out, iter.requested);

    exist = iter.buffer != nullptr;
    BACKUP_SCALAR(out, exist);

    if (exist) {
      BACKUP_BLOB(out, iter.buffer, iter.requested);
    }

    BACKUP_SCALAR(out, iter.regionIndex);
  }

  size = pendingInitTagList.size();
  BACKUP_SCALAR(out, size);

  for (auto &iter : pendingInitTagList) {
    BACKUP_DMATAG(out, iter.parent);
    BACKUP_EVENT(out, iter.eid);
    BACKUP_SCALAR(out, iter.data);

    BACKUP_SCALAR(out, iter.handled);
    BACKUP_SCALAR(out, iter.requested);
    BACKUP_SCALAR(out, iter.bufferSize);

    if (iter.bufferSize > 0) {
      BACKUP_BLOB(out, iter.buffer, iter.bufferSize);
    }
  }
}

void DMAEngine::restoreCheckpoint(std::istream &in) noexcept {
  bool exist;

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
    DMASession session(InvalidDMATag, InvalidEventID);

    RESTORE_SCALAR(in, oldTag);
    oldTag = restoreDMATag(oldTag);

    session.parent = oldTag;

    RESTORE_EVENT(in, session.eid);
    RESTORE_SCALAR(in, session.data);

    RESTORE_SCALAR(in, session.read);
    RESTORE_SCALAR(in, session.handled);
    RESTORE_SCALAR(in, session.requested);

    RESTORE_SCALAR(in, exist);

    if (exist) {
      session.buffer = (uint8_t *)calloc(session.requested, 1);
      RESTORE_BLOB(in, session.buffer, session.requested);
    }

    RESTORE_SCALAR(in, session.regionIndex);

    pendingTagList.emplace_back(session);
  }

  RESTORE_SCALAR(in, size);

  for (uint64_t i = 0; i < size; i++) {
    DMASession session(InvalidDMATag, InvalidEventID);

    RESTORE_SCALAR(in, oldTag);
    oldTag = restoreDMATag(oldTag);

    session.parent = oldTag;

    RESTORE_EVENT(in, session.eid);
    RESTORE_SCALAR(in, session.data);

    RESTORE_SCALAR(in, session.handled);
    RESTORE_SCALAR(in, session.requested);
    RESTORE_SCALAR(in, session.bufferSize);

    if (session.bufferSize > 0) {
      RESTORE_BLOB(in, session.buffer, session.bufferSize);
    }

    pendingInitTagList.emplace_back(session);
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
