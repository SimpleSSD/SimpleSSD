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

DMAData::DMAData() : inited(false) {}

bool DMAData::isInited() noexcept {
  return inited;
}

DMAEngine::DMASession::DMASession(uint64_t i, DMATag t, Event e, uint64_t d,
                                  uint64_t s, uint8_t *b)
    : tag(i),
      parent(t),
      eid(e),
      data(d),
      handled(0),
      requested(s),
      bufferSize(0),
      regionIndex(0),
      buffer(b) {}

void DMAEngine::DMASession::allocateBuffer(uint32_t size) {
  bufferSize = size;
  buffer = (uint8_t *)calloc(size, 1);
}

void DMAEngine::DMASession::deallocateBuffer() {
  free(buffer);

  bufferSize = 0;
  buffer = nullptr;
}

DMAEngine::DMAEngine(ObjectData &o, DMAInterface *i)
    : Object(o), interface(i), sessionID(0), pageSize(0) {
  eventReadDMADone =
      createEvent([this](uint64_t, uint64_t d) { dmaReadDone(d); },
                  "HIL::DMAEngine::eventReadDMADone");
  eventWriteDMADone =
      createEvent([this](uint64_t, uint64_t d) { dmaWriteDone(d); },
                  "HIL::DMAEngine::eventWriteDMADone");
  eventPRDTInitDone =
      createEvent([this](uint64_t, uint64_t d) { prdt_readDone(d); },
                  "HIL::DMAEngine::eventPRDTInitDone");
  eventPRPReadDone = createEvent(
      [this](uint64_t, uint64_t d) { getPRPListFromPRP_readDone(d); },
      "HIL::DMAEngine::eventPRPReadDone");
  eventSGLReadDone =
      createEvent([this](uint64_t, uint64_t d) { parseSGLSegment_readDone(d); },
                  "HIL::DMAEngine::eventSGLReadDone");
}

DMAEngine::~DMAEngine() {
  warn_if(sessionList.size() > 0,
          "Not all DMA Session destroyed (%" PRIu64 " left).",
          sessionList.size());

  sessionList.clear();

  warn_if(tagList.size() > 0, "Not all DMA Tag destroyed (%" PRIu64 " left).",
          tagList.size());

  for (auto &iter : tagList) {
    delete iter;
  }

  tagList.clear();
}

void DMAEngine::dmaReadDone(uint64_t tag) {
  auto &session = findSession(tag);

  if (session.handled == session.requested) {
    scheduleNow(session.eid, session.data);

    destroySession(tag);
  }
  else {
    readNext(session);
  }
}

void DMAEngine::dmaWriteDone(uint64_t tag) {
  auto &session = findSession(tag);

  if (session.handled == session.requested) {
    scheduleNow(session.eid, session.data);

    destroySession(tag);
  }
  else {
    writeNext(session);
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

void DMAEngine::prdt_readDone(uint64_t tag) {
  auto &session = findSession(tag);

  // Fill vector
  PRDT *prdt = (PRDT *)session.buffer;
  uint32_t entryCount = (uint32_t)(session.bufferSize / sizeof(PRDT));

  for (uint32_t i = 0; i < entryCount; i++) {
    session.parent->prList.emplace_back(
        PhysicalRegion(prdt->address, prdt->size + 1));
    session.handled += prdt->size + 1;
  }

  session.deallocateBuffer();
  session.parent->inited = true;

  scheduleNow(session.eid, session.data);

  destroySession(tag);
}

uint32_t DMAEngine::getPRPSize(uint64_t prp) {
  return (uint32_t)(pageSize - (prp & (pageSize - 1)));
}

void DMAEngine::getPRPListFromPRP(DMASession &session, uint64_t prp) {
  session.allocateBuffer(getPRPSize(prp));

  interface->read(prp, session.bufferSize, session.buffer, eventPRPReadDone,
                  session.tag);
}

void DMAEngine::getPRPListFromPRP_readDone(uint64_t tag) {
  auto &session = findSession(tag);
  uint64_t listPRP;
  uint32_t listPRPSize;

  for (size_t i = 0; i < session.bufferSize; i += 8) {
    listPRP = *((uint64_t *)(session.buffer + i));
    listPRPSize = getPRPSize(listPRP);

    if (listPRP == 0) {
      panic("Invalid PRP in PRP List");
    }

    session.parent->prList.emplace_back(PhysicalRegion(listPRP, listPRPSize));
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
    listPRPSize = getPRPSize(listPRP);

    session.parent->prList.pop_back();
    session.handled -= listPRPSize;

    getPRPListFromPRP(session, listPRP);
  }
  else {
    session.parent->inited = true;

    scheduleNow(session.eid, session.data);

    destroySession(tag);
  }
}

void DMAEngine::parseSGLDescriptor(DMASession &session, SGLDescriptor *desc) {
  switch (desc->getType()) {
    case DataBlock:
    case KeyedDataBlock:
      session.parent->prList.emplace_back(
          PhysicalRegion(desc->address, desc->length));
      session.handled += desc->length;

      break;
    case BitBucket:
      session.parent->prList.emplace_back(
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

void DMAEngine::parseSGLSegment(DMASession &session, uint64_t address,
                                uint32_t length) {
  session.allocateBuffer(length);

  interface->read(address, session.bufferSize, session.buffer, eventSGLReadDone,
                  session.tag);
}

void DMAEngine::parseSGLSegment_readDone(uint64_t tag) {
  auto &session = findSession(tag);
  bool next = false;

  SGLDescriptor *desc;

  for (uint32_t i = 0; i < session.bufferSize; i += 16) {
    desc = (SGLDescriptor *)(session.buffer + i);

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
    parseSGLSegment(session, desc->address, desc->length);
  }
  else {
    session.parent->inited = true;

    scheduleNow(session.eid, session.data);

    destroySession(tag);
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
  auto &siter = createSession(ret, eid, data, size, nullptr);
  auto &session = siter.second;

  session.allocateBuffer(size);

  interface->read(base, size, session.buffer, eventPRDTInitDone, siter.first);

  return ret;
}

DMATag DMAEngine::initFromPRP(uint64_t prp1, uint64_t prp2, uint32_t size,
                              Event eid, uint64_t data) noexcept {
  panic_if(popcount64(pageSize) != 1, "Invalid memory page size provided.");

  auto ret = createTag();

  bool immediate = true;
  uint8_t mode = 0xFF;
  uint32_t prp1Size = getPRPSize(prp1);
  uint32_t prp2Size = getPRPSize(prp2);

  auto &siter = createSession(ret, eid, data, size, nullptr);
  auto &session = siter.second;

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
      ret->prList.emplace_back(PhysicalRegion(prp1, prp1Size));
      session.handled = prp1Size;

      break;
    case 1:
      // PRP1 and PRP2 are PRP pointer
      ret->prList.emplace_back(PhysicalRegion(prp1, prp1Size));
      ret->prList.emplace_back(PhysicalRegion(prp2, prp2Size));

      panic_if(prp1Size + prp2Size < session.requested, "Invalid DPTR size");

      session.requested = prp1Size + prp2Size;

      break;
    case 2:
      // PRP1 is PRP pointer, PRP2 is PRP list
      ret->prList.emplace_back(PhysicalRegion(prp1, prp1Size));

      /* fallthrough */
    case 3:
      // Prepare for read
      immediate = false;
      session.handled = prp1Size;

      getPRPListFromPRP(session, prp2);

      break;
    default:
      panic("Invalid PRP1/2 while parsing.");

      break;
  }

  if (immediate) {
    session.parent->inited = true;

    scheduleNow(eid, data);

    destroySession(siter.first);
  }

  return ret;
}

DMATag DMAEngine::initFromSGL(uint64_t dptr1, uint64_t dptr2, uint32_t size,
                              Event eid, uint64_t data) noexcept {
  auto ret = createTag();

  SGLDescriptor desc;

  auto &siter = createSession(ret, eid, data, size, nullptr);
  auto &session = siter.second;

  // Create first SGL descriptor from PRP pointers
  desc.dptr1 = dptr1;
  desc.dptr2 = dptr2;

  // Check type
  if (desc.getType() == SGLDescriptorType::DataBlock ||
      desc.getType() == SGLDescriptorType::KeyedDataBlock) {
    // This is entire buffer
    parseSGLDescriptor(session, &desc);

    session.parent->inited = true;

    scheduleNow(eid, data);

    destroySession(siter.first);
  }
  else if (desc.getType() == SGLDescriptorType::Segment ||
           desc.getType() == SGLDescriptorType::LastSegment) {
    parseSGLSegment(session, desc.address, desc.length);
  }
  else {
    panic("Unexpected SGL descriptor type.");
  }

  return ret;
}

DMATag DMAEngine::initRaw(uint64_t base, uint32_t size) noexcept {
  auto ret = createTag();

  ret->prList.emplace_back(PhysicalRegion(base, size));
  ret->inited = true;

  return ret;
}

void DMAEngine::deinit(DMATag tag) noexcept {
  destroyTag(tag);
}

void DMAEngine::readNext(DMASession &session) noexcept {
  uint32_t read;
  bool submit = false;

  auto &iter = session.parent->prList.at(++session.regionIndex);

  read = MIN(iter.size, session.requested - session.handled);

  if (!iter.ignore) {
    submit = true;

    interface->read(iter.address, read,
                    session.buffer ? session.buffer + session.handled : nullptr,
                    eventReadDMADone, session.tag);
  }

  session.handled += read;

  if (!submit) {
    dmaReadDone(session.tag);
  }
}

void DMAEngine::read(DMATag tag, uint64_t offset, uint32_t size,
                     uint8_t *buffer, Event eid, uint64_t data) noexcept {
  panic_if(!tag, "Accessed to uninitialized DMAEngine.");

  uint64_t currentOffset = 0;
  uint32_t read;
  bool submit = false;

  auto &siter = createSession(tag, eid, data, size, buffer);
  auto &session = siter.second;

  for (session.regionIndex = 0; session.regionIndex < tag->prList.size();
       session.regionIndex++) {
    auto &iter = tag->prList.at(session.regionIndex);

    if (currentOffset + iter.size > offset) {
      session.handled = offset - currentOffset;

      read = MIN(iter.size - session.handled, size);

      if (!iter.ignore) {
        interface->read(iter.address + session.handled, read, session.buffer,
                        eventReadDMADone, siter.first);

        submit = true;
      }

      session.handled = read;

      break;
    }

    currentOffset += iter.size;
  }

  if (!submit) {
    dmaReadDone(siter.first);
  }
}

void DMAEngine::writeNext(DMASession &session) noexcept {
  uint32_t written;
  bool submit = false;

  auto &iter = session.parent->prList.at(++session.regionIndex);

  written = MIN(iter.size, session.requested - session.handled);

  if (!iter.ignore) {
    submit = true;

    interface->write(
        iter.address, written,
        session.buffer ? session.buffer + session.handled : nullptr,
        eventWriteDMADone, session.tag);
  }

  session.handled += written;

  if (!submit) {
    dmaWriteDone(session.tag);
  }
}

void DMAEngine::write(DMATag tag, uint64_t offset, uint32_t size,
                      uint8_t *buffer, Event eid, uint64_t data) noexcept {
  panic_if(!tag, "Accessed to uninitialized DMAEngine.");

  uint64_t currentOffset = 0;
  uint32_t written;
  bool submit = false;

  auto &siter = createSession(tag, eid, data, size, buffer);
  auto &session = siter.second;

  for (session.regionIndex = 0; session.regionIndex < tag->prList.size();
       session.regionIndex++) {
    auto &iter = tag->prList.at(session.regionIndex);

    if (currentOffset + iter.size > offset) {
      session.handled = offset - currentOffset;

      written = MIN(iter.size - session.handled, size);

      if (!iter.ignore) {
        submit = true;

        interface->write(iter.address + session.handled, written,
                         session.buffer, eventWriteDMADone, siter.first);
      }

      session.handled = written;

      break;
    }

    currentOffset += iter.size;
  }

  if (!submit) {
    dmaWriteDone(siter.first);
  }
}

DMAEngine::DMASession &DMAEngine::findSession(uint64_t tag) {
  auto iter = sessionList.find(tag);

  panic_if(iter == sessionList.end(), "Unexpected DMA session ID.");

  return iter->second;
}

std::pair<const uint64_t, DMAEngine::DMASession> &DMAEngine::createSession(
    DMATag t, Event e, uint64_t d = 0, uint64_t s = 0, uint8_t *b = nullptr) {
  uint64_t tag = sessionID++;
  auto iter = sessionList.emplace(tag, DMASession(tag, t, e, d, s, b));

  panic_if(!iter.second, "Failed to create DMA session.");

  return *iter.first;
}

void DMAEngine::destroySession(uint64_t tag) {
  auto iter = sessionList.find(tag);

  panic_if(iter == sessionList.end(), "Unexpected DMA session ID.");

  sessionList.erase(iter);
}

void DMAEngine::getStatList(std::vector<Stat> &, std::string) noexcept {}

void DMAEngine::getStatValues(std::vector<double> &) noexcept {}

void DMAEngine::resetStatValues() noexcept {}

void DMAEngine::createCheckpoint(std::ostream &out) const noexcept {
  BACKUP_EVENT(out, eventReadDMADone);
  BACKUP_EVENT(out, eventWriteDMADone);
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

  {
    /**
     * DMA Sessions cannot be checkpointed -- serialized. Because it has buffer
     * pointer, which may have different value after restarting simulation.
     * The gem5 provides concept of Drain - DrainState, drain() function of
     * SimObject. All pending DMA operations should be drained before
     * checkpoint -- serialized.
     */
    if (UNLIKELY(sessionList.size() != 0)) {
      // Remove const qualifier
      auto pthis = const_cast<DMAEngine *>(this);

      pthis->panic_log("%s:%u: %s\n  "
                       "Sessions cannot be checkpointed.",
                       __FILENAME__, __LINE__, __PRETTY_FUNCTION__);
    }
  }
}

void DMAEngine::restoreCheckpoint(std::istream &in) noexcept {
  RESTORE_EVENT(in, eventReadDMADone);
  RESTORE_EVENT(in, eventWriteDMADone);
  RESTORE_EVENT(in, eventPRDTInitDone);
  RESTORE_EVENT(in, eventPRPReadDone);
  RESTORE_EVENT(in, eventSGLReadDone);

  uint64_t size;
  DMATag oldTag, newTag;

  RESTORE_SCALAR(in, size);

  oldTagList.reserve(size);

  for (uint64_t i = 0; i < size; i++) {
    newTag = new DMAData();

    RESTORE_SCALAR(in, oldTag);

    uint64_t list;
    RESTORE_SCALAR(in, list);

    newTag->prList.reserve(list);

    for (uint64_t j = 0; j < list; j++) {
      PhysicalRegion pr;

      RESTORE_SCALAR(in, pr.address);
      RESTORE_SCALAR(in, pr.size);
      RESTORE_SCALAR(in, pr.ignore);

      newTag->prList.emplace_back(pr);
    }

    tagList.emplace(newTag);
    oldTagList.emplace(oldTag, newTag);
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
